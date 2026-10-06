#include "e1cipher/runtime/cost_model.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "e1cipher/crypto/kernels/ntt.hpp"
#include "e1cipher/fusion/feature_vector.hpp"

namespace e1cipher::runtime {

namespace {

constexpr int kCalibrationTrials = 15;

template <typename Fn>
double time_ms(Fn&& fn, int trials) {
    for (int i = 0; i < std::max(2, trials / 5); ++i) fn();  // warm-up
    std::vector<double> samples(static_cast<std::size_t>(trials));
    for (int i = 0; i < trials; ++i) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        samples[static_cast<std::size_t>(i)] =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];  // median
}

}  // namespace

CostModel::CostModel(const crypto::CkksParamSet& param_set) : param_set_(param_set) {
    ckks_backend_ = crypto::CkksBackend::create(param_set_);
    if (!ckks_backend_) {
        throw std::runtime_error("CostModel: failed to construct a CKKS backend for parameter set " +
                                 std::string(param_set_.id));
    }

    // --- calibrate LOCAL: a byte-copy/accumulate proxy for on-device compute ---
    {
        std::vector<double> buf(2048, 1.0);
        volatile double sink = 0;  // prevent the optimizer from deleting the loop
        const double ms = time_ms([&] { sink = std::accumulate(buf.begin(), buf.end(), 0.0); }, kCalibrationTrials);
        (void)sink;
        calibrated_local_ns_per_byte_ = (ms * 1e6) / static_cast<double>(buf.size() * sizeof(double));
    }

    // --- calibrate ENCRYPTED: a real CKKS encrypt + add, at this operation's natural scale ---
    {
        fusion::FeatureVector v;
        for (std::size_t i = 0; i < fusion::kFeatureCount; ++i) v.values[i] = static_cast<double>(i) * 0.1;

        crypto::EncryptedVector enc;
        calibrated_encrypt_ms_ = time_ms([&] { enc = ckks_backend_->encrypt(v); }, kCalibrationTrials);
        ckks_ciphertext_bytes_ = enc.byte_length();

        crypto::EncryptedVector enc2 = ckks_backend_->encrypt(v);
        calibrated_add_ms_ = time_ms(
            [&] {
                auto out = ckks_backend_->add(enc, enc2);
                (void)out;
            },
            kCalibrationTrials);
    }
}

CostEstimate CostModel::estimate_local(const Operation& op) const {
    CostEstimate c;
    c.latency_ms = {calibrated_local_ns_per_byte_ * static_cast<double>(op.plaintext_bytes) / 1e6,
                    platform::ResultClass::HostReference};
    c.bandwidth_bytes = {0, platform::ResultClass::HostReference};  // nothing transmitted
    c.memory_bytes = {static_cast<double>(op.plaintext_bytes), platform::ResultClass::HostReference};
    return c;
}

CostEstimate CostModel::estimate_encrypted(const Operation& op) const {
    (void)op;
    CostEstimate c;
    c.latency_ms = {calibrated_encrypt_ms_ + calibrated_add_ms_, platform::ResultClass::HostReference};
    c.bandwidth_bytes = {static_cast<double>(ckks_ciphertext_bytes_), platform::ResultClass::HostReference};
    c.memory_bytes = {static_cast<double>(ckks_ciphertext_bytes_), platform::ResultClass::HostReference};
    return c;
}

CostEstimate CostModel::estimate_remote(const Operation& op, const HardwareProfile& hw) const {
    CostEstimate c;
    const double mbps = std::max(hw.network_mbps, 0.01);
    const double seconds = (static_cast<double>(op.plaintext_bytes) * 8.0) / (mbps * 1e6);
    c.latency_ms = {seconds * 1000.0, platform::ResultClass::Estimated};
    c.bandwidth_bytes = {static_cast<double>(op.plaintext_bytes), platform::ResultClass::HostReference};
    c.memory_bytes = {static_cast<double>(op.plaintext_bytes), platform::ResultClass::HostReference};
    return c;
}

CostEstimate CostModel::estimate_accelerated(const Operation& op) const {
    // Candidate-kernel proxy: run the real NTT reference kernel (host) at a
    // size matching this operation, as a stand-in for "what an E1 NTT
    // kernel would need to process." This is a genuine host measurement of
    // the KERNEL'S shape -- it is not an E1 measurement and claims no
    // speedup; see docs/e1-application-mapping.md.
    const std::size_t n = std::bit_ceil(std::max<std::size_t>(op.vector_length, 16));
    std::mt19937_64 rng(1);
    std::uniform_int_distribution<std::uint64_t> dist(0, crypto::kernels::kNttPrime - 1);
    std::vector<std::uint64_t> data(n);
    for (auto& v : data) v = dist(rng);

    const double ms = time_ms(
        [&] {
            auto result = crypto::kernels::ntt(data, false);
            (void)result;
        },
        kCalibrationTrials);

    CostEstimate c;
    c.latency_ms = {ms, platform::ResultClass::HostReference};
    // The accelerated path still ultimately produces a CKKS-sized
    // ciphertext once the surrounding encrypt/key-switch wraps it -- reuse
    // that real measurement for bandwidth/memory rather than inventing a
    // second number.
    c.bandwidth_bytes = {static_cast<double>(ckks_ciphertext_bytes_), platform::ResultClass::HostReference};
    c.memory_bytes = {static_cast<double>(ckks_ciphertext_bytes_), platform::ResultClass::HostReference};
    return c;
}

}  // namespace e1cipher::runtime

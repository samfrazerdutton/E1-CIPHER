// CKKS operation benchmark suite (project brief section 10). For every
// parameter set in include/e1cipher/crypto/ckks_params.hpp, measures real
// wall-clock latency (std::chrono::steady_clock -- a HOST REFERENCE
// measurement, never an E1 measurement) for encode, encrypt, add,
// multiply, relinearize, rescale, rotate, decrypt, decode. Ports
// tools/bench/ckks-bench.ts.
//
// N1024 has no key-switching support (single-modulus chain) -- multiply/
// relinearize/rescale/rotate are reported as explicitly unsupported, not
// silently skipped or crashed past.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include <seal/seal.h>

#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/platform/host_platform.hpp"

using namespace e1cipher;
using namespace e1cipher::crypto;

namespace {

constexpr int kTrials = 30;

struct TimingStats {
    double mean_ms = 0, median_ms = 0, stddev_ms = 0, min_ms = 0, max_ms = 0;
};

template <typename Fn>
TimingStats time_op(Fn&& fn, int trials = kTrials) {
    const int warmup = std::max(3, trials / 5);
    for (int i = 0; i < warmup; ++i) fn();

    std::vector<double> samples(static_cast<std::size_t>(trials));
    for (int i = 0; i < trials; ++i) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        samples[static_cast<std::size_t>(i)] =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    TimingStats stats;
    stats.mean_ms = std::accumulate(samples.begin(), samples.end(), 0.0) / trials;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    stats.median_ms = sorted[static_cast<std::size_t>(trials / 2)];
    stats.min_ms = sorted.front();
    stats.max_ms = sorted.back();
    double var = 0;
    for (double s : samples) var += (s - stats.mean_ms) * (s - stats.mean_ms);
    stats.stddev_ms = std::sqrt(var / trials);
    return stats;
}

struct OpResult {
    bool supported = false;
    std::string reason;
    TimingStats timing;
};

std::vector<double> make_test_vector(std::size_t n, std::uint32_t seed) {
    std::uint32_t state = seed;
    std::vector<double> out(n);
    for (auto& v : out) {
        state = static_cast<std::uint32_t>(state * 1103515245u + 12345u);
        v = (static_cast<double>(state) / 4294967296.0) * 2.0 - 1.0;
    }
    return out;
}

double max_abs_error(const std::vector<double>& a, const std::vector<double>& b, std::size_t n) {
    double m = 0;
    for (std::size_t i = 0; i < n; ++i) m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}

struct ParamSetResult {
    const CkksParamSet* params = nullptr;
    bool parameters_set = false;
    int max_bits_at_128 = 0;
    std::size_t slot_count = 0;
    std::map<std::string, OpResult> ops;
    std::size_t ciphertext_bytes = 0;
    double roundtrip_max_abs_err = -1;
    double multiply_max_abs_err = -1;
};

ParamSetResult benchmark_param_set(const CkksParamSet& p) {
    ParamSetResult result;
    result.params = &p;

    seal::EncryptionParameters parms(seal::scheme_type::ckks);
    parms.set_poly_modulus_degree(p.poly_modulus_degree);
    std::vector<int> bits(p.coeff_modulus_bits.begin(), p.coeff_modulus_bits.end());
    parms.set_coeff_modulus(seal::CoeffModulus::Create(p.poly_modulus_degree, bits));

    seal::SEALContext context(parms);
    result.parameters_set = context.parameters_set();
    result.max_bits_at_128 = seal::CoeffModulus::MaxBitCount(p.poly_modulus_degree, seal::sec_level_type::tc128);
    if (!result.parameters_set) return result;

    seal::KeyGenerator keygen(context);
    seal::SecretKey secret_key = keygen.secret_key();
    seal::PublicKey public_key;
    keygen.create_public_key(public_key);
    const bool using_ks = context.using_keyswitching();
    seal::RelinKeys relin_keys;
    seal::GaloisKeys galois_keys;
    if (using_ks) {
        keygen.create_relin_keys(relin_keys);
        keygen.create_galois_keys(galois_keys);
    }

    seal::CKKSEncoder encoder(context);
    seal::Encryptor encryptor(context, public_key);
    seal::Decryptor decryptor(context, secret_key);
    seal::Evaluator evaluator(context);

    const double scale = std::pow(2.0, p.scale_bits);
    result.slot_count = encoder.slot_count();

    auto data1 = make_test_vector(result.slot_count, 42);
    auto data2 = make_test_vector(result.slot_count, 43);

    seal::Plaintext plain1;
    result.ops["encode"] = {true, "", time_op([&] { encoder.encode(data1, scale, plain1); })};
    seal::Plaintext plain2;
    encoder.encode(data2, scale, plain2);

    seal::Ciphertext cipher1;
    result.ops["encrypt"] = {true, "", time_op([&] { encryptor.encrypt(plain1, cipher1); })};
    seal::Ciphertext cipher2;
    encryptor.encrypt(plain2, cipher2);

    result.ops["add"] = {true, "", time_op([&] {
                             seal::Ciphertext out;
                             evaluator.add(cipher1, cipher2, out);
                         })};

    seal::Ciphertext product_for_accuracy;
    bool have_product = false;

    if (using_ks) {
        result.ops["multiply"] = {true, "", time_op([&] {
                                      seal::Ciphertext out;
                                      evaluator.multiply(cipher1, cipher2, out);
                                  })};

        auto fresh_product = [&] {
            seal::Ciphertext out;
            evaluator.multiply(cipher1, cipher2, out);
            return out;
        };

        result.ops["relinearize"] = {true, "", time_op([&] {
                                         auto p2 = fresh_product();
                                         evaluator.relinearize_inplace(p2, relin_keys);
                                     })};

        result.ops["rescale"] = {true, "", time_op([&] {
                                     auto p2 = fresh_product();
                                     evaluator.relinearize_inplace(p2, relin_keys);
                                     evaluator.rescale_to_next_inplace(p2);
                                 })};

        product_for_accuracy = fresh_product();
        evaluator.relinearize_inplace(product_for_accuracy, relin_keys);
        evaluator.rescale_to_next_inplace(product_for_accuracy);
        have_product = true;

        result.ops["rotate"] = {true, "", time_op([&] {
                                    seal::Ciphertext out;
                                    evaluator.rotate_vector(cipher1, 1, galois_keys, out);
                                })};
    } else {
        const std::string reason = "Parameter set has no key-switching support (single-modulus chain, " +
                                   std::to_string(result.max_bits_at_128) +
                                   "-bit budget); relin/Galois keys cannot be generated.";
        result.ops["multiply"] = {false, reason, {}};
        result.ops["relinearize"] = {false, reason, {}};
        result.ops["rescale"] = {false, reason, {}};
        result.ops["rotate"] = {false, reason, {}};
    }

    result.ops["decrypt"] = {true, "", time_op([&] {
                                 seal::Plaintext out;
                                 decryptor.decrypt(cipher1, out);
                             })};
    seal::Plaintext decrypted_plain;
    decryptor.decrypt(cipher1, decrypted_plain);
    result.ops["decode"] = {true, "", time_op([&] {
                                std::vector<double> out;
                                encoder.decode(decrypted_plain, out);
                            })};

    {
        std::ostringstream oss(std::ios::binary);
        cipher1.save(oss, seal::compr_mode_type::none);
        result.ciphertext_bytes = oss.str().size();
    }

    std::vector<double> roundtrip;
    encoder.decode(decrypted_plain, roundtrip);
    result.roundtrip_max_abs_err = max_abs_error(data1, roundtrip, result.slot_count);

    if (have_product) {
        seal::Plaintext prod_plain;
        decryptor.decrypt(product_for_accuracy, prod_plain);
        std::vector<double> prod_decoded;
        encoder.decode(prod_plain, prod_decoded);
        std::vector<double> expected(result.slot_count);
        for (std::size_t i = 0; i < result.slot_count; ++i) expected[i] = data1[i] * data2[i];
        result.multiply_max_abs_err = max_abs_error(expected, prod_decoded, result.slot_count);
    }

    return result;
}

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

}  // namespace

int main() {
    const auto host = platform::capture_host_info();
    std::printf("CKKS benchmark suite (C++ / Microsoft SEAL)\n");
    std::printf("Host: %s, %u threads, %s %s, commit %s\n", host.cpu_model.c_str(), host.hardware_threads,
                host.compiler_id.c_str(), host.compiler_version.c_str(), host.git_commit.c_str());
    std::printf("All timings below are HOST REFERENCE RESULTS -- not E1 silicon measurements.\n\n");

    std::vector<ParamSetResult> results;
    for (const auto& p : ckks_param_sets()) {
        std::printf("--- %s (N=%zu) ---\n", std::string(p.id).c_str(), p.poly_modulus_degree);
        auto r = benchmark_param_set(p);
        if (!r.parameters_set) {
            std::printf("  FAILED: parameters_set()=false (budget %d bits)\n\n", r.max_bits_at_128);
            results.push_back(std::move(r));
            continue;
        }
        std::printf("  slotCount=%zu ciphertextBytes=%zu\n", r.slot_count, r.ciphertext_bytes);
        for (const char* op :
             {"encode", "encrypt", "add", "multiply", "relinearize", "rescale", "rotate", "decrypt", "decode"}) {
            const auto& res = r.ops.at(op);
            if (res.supported) {
                std::printf("  %-12s mean=%.3fms median=%.3fms stddev=%.3fms\n", op, res.timing.mean_ms,
                            res.timing.median_ms, res.timing.stddev_ms);
            } else {
                std::printf("  %-12s N/A -- %s\n", op, res.reason.c_str());
            }
        }
        std::printf("  accuracy: roundtrip maxAbsErr=%.3e", r.roundtrip_max_abs_err);
        if (r.multiply_max_abs_err >= 0) std::printf(", multiply maxAbsErr=%.3e", r.multiply_max_abs_err);
        std::printf("\n\n");
        results.push_back(std::move(r));
    }

    std::filesystem::create_directories("results");
    std::ofstream out("results/cpp-ckks-bench-latest.json");
    out << "{\n  \"host\": {\n";
    out << "    \"cpuModel\": \"" << json_escape(host.cpu_model) << "\",\n";
    out << "    \"compiler\": \"" << json_escape(host.compiler_id) << " " << json_escape(host.compiler_version)
        << "\",\n";
    out << "    \"gitCommit\": \"" << json_escape(host.git_commit) << "\",\n";
    out << "    \"resultClass\": \"HOST_REFERENCE\"\n  },\n";
    out << "  \"results\": [\n";
    for (std::size_t ri = 0; ri < results.size(); ++ri) {
        const auto& r = results[ri];
        out << "    {\n      \"id\": \"" << r.params->id
            << "\",\n      \"parametersSet\": " << (r.parameters_set ? "true" : "false")
            << ",\n      \"maxBitsAt128\": " << r.max_bits_at_128 << ",\n      \"slotCount\": " << r.slot_count
            << ",\n      \"ciphertextBytes\": " << r.ciphertext_bytes
            << ",\n      \"roundtripMaxAbsErr\": " << r.roundtrip_max_abs_err << ",\n      \"ops\": {\n";
        std::size_t oi = 0;
        for (const auto& [name, res] : r.ops) {
            out << "        \"" << name << "\": ";
            if (res.supported) {
                out << "{ \"meanMs\": " << res.timing.mean_ms << ", \"medianMs\": " << res.timing.median_ms
                    << ", \"stdDevMs\": " << res.timing.stddev_ms << " }";
            } else {
                out << "{ \"supported\": false, \"reason\": \"" << json_escape(res.reason) << "\" }";
            }
            out << (++oi < r.ops.size() ? ",\n" : "\n");
        }
        out << "      }\n    }" << (ri + 1 < results.size() ? ",\n" : "\n");
    }
    out << "  ]\n}\n";
    std::printf("Wrote results/cpp-ckks-bench-latest.json\n");

    return 0;
}

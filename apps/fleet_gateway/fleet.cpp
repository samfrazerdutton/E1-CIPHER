#include "fleet.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include "e1cipher/crypto/ckks_backend.hpp"
#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/crypto/plaintext_backend.hpp"
#include "e1cipher/fusion/feature_vector.hpp"
#include "e1cipher/telemetry/generator.hpp"

namespace e1cipher::apps {

namespace {

double elapsed_ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int run_fleet(const FleetOptions& opts) {
    const auto& sets = crypto::ckks_param_sets();
    const auto it = std::find_if(sets.begin(), sets.end(), [](const auto& p) { return p.id == "N4096"; });
    if (it == sets.end()) {
        std::puts("CKKS parameter set N4096 not found -- aborting.");
        return 1;
    }

    std::puts("Fleet gateway: plaintext vs CKKS aggregation");
    std::printf("CKKS parameter set: %s (N=%zu)\n", std::string(it->id).c_str(), it->poly_modulus_degree);
    std::puts("Gateway role below constructs no SecretKey/Decryptor -- see fleet.hpp.\n");

    // "Analysis endpoint" role: holds the only CkksBackend (and therefore
    // the only SecretKey) in this process. The fleet-size loop below only
    // ever gives the "gateway" role EncryptedVector objects and
    // CryptoBackend::add(); only the analysis endpoint calls decrypt().
    auto analysis_endpoint_backend = crypto::CkksBackend::create(*it);
    if (!analysis_endpoint_backend) {
        std::puts("Failed to construct CKKS backend -- aborting.");
        return 1;
    }
    crypto::PlaintextBackend plaintext_backend;

    const std::vector<std::uint32_t> fleet_sizes = {1, 5, 10, 25, 50};

    std::printf("%-10s %14s %12s %14s %10s %14s\n", "fleet", "plaintext_ms", "plain_bytes", "ckks_crit_ms", "ratio",
                "max_abs_err");
    for (std::uint32_t fleet_size : fleet_sizes) {
        telemetry::FleetTickOptions tick_opts{opts.seed, fleet_size, opts.tick, opts.scenario,
                                              telemetry::kDefaultHomeBase};
        auto samples = telemetry::generate_fleet_tick(tick_opts);

        std::vector<fusion::FeatureVector> features;
        features.reserve(samples.size());
        for (const auto& s : samples) features.push_back(fusion::extract_feature_vector(s));

        // --- plaintext path ---
        const auto plain_start = std::chrono::steady_clock::now();
        crypto::EncryptedVector plain_acc = plaintext_backend.encrypt(features[0]);
        for (std::size_t i = 1; i < features.size(); ++i) {
            plain_acc = plaintext_backend.add(plain_acc, plaintext_backend.encrypt(features[i]));
        }
        const auto plain_sum = plaintext_backend.decrypt(plain_acc);
        const double plaintext_ms = elapsed_ms(plain_start);
        const std::size_t plaintext_bytes = plaintext_backend.encrypt(features[0]).byte_length() * features.size();

        // --- CKKS path, gateway role: encrypt (on-device, not timed as "gateway work") + aggregate ---
        std::vector<crypto::EncryptedVector> ciphers;
        ciphers.reserve(features.size());
        const auto encrypt_one_start = std::chrono::steady_clock::now();
        ciphers.push_back(analysis_endpoint_backend->encrypt(features[0]));
        const double encrypt_critical_path_ms = elapsed_ms(encrypt_one_start);
        for (std::size_t i = 1; i < features.size(); ++i)
            ciphers.push_back(analysis_endpoint_backend->encrypt(features[i]));

        crypto::EncryptedVector cipher_acc = ciphers[0];
        for (std::size_t i = 1; i < ciphers.size(); ++i)
            cipher_acc = analysis_endpoint_backend->add(cipher_acc, ciphers[i]);

        // --- analysis endpoint role: the only place decrypt() is called ---
        const auto ckks_sum = analysis_endpoint_backend->decrypt(cipher_acc);
        const std::size_t ckks_bytes = ciphers[0].byte_length() * ciphers.size();

        double max_err = 0.0;
        for (std::size_t i = 0; i < fusion::kFeatureCount; ++i) {
            max_err =
                std::max(max_err, std::abs(ckks_sum.values[i] - plain_sum.values[i]) / static_cast<double>(fleet_size));
        }

        std::printf("%-10u %14.4f %12zu %14.4f %9.0fx %14.3e\n", fleet_size, plaintext_ms, plaintext_bytes,
                    encrypt_critical_path_ms, static_cast<double>(ckks_bytes) / static_cast<double>(plaintext_bytes),
                    max_err);
    }

    return 0;
}

}  // namespace e1cipher::apps

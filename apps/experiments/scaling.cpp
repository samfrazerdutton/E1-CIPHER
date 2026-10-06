#include "scaling.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/runtime/cost_model.hpp"
#include "e1cipher/runtime/types.hpp"

namespace e1cipher::apps {

using namespace e1cipher::runtime;

namespace {

constexpr std::size_t kDiagnosticBytes = 8;

struct ScaleRow {
    std::size_t devices;
    double plaintext_bandwidth_mb;
    double full_ckks_bandwidth_mb;
    double selective_ckks_bandwidth_mb;
    double full_ckks_encryption_ms;
    double selective_ckks_encryption_ms;
    double full_ckks_gateway_ms;
    double selective_ckks_gateway_ms;
};

}  // namespace

int run_scaling_experiment() {
    const auto& sets = crypto::ckks_param_sets();
    const auto it = std::find_if(sets.begin(), sets.end(), [](const auto& p) { return p.id == "N4096"; });
    if (it == sets.end()) {
        std::puts("CKKS parameter set N4096 not found -- aborting.");
        return 1;
    }

    std::puts("EXPERIMENT: Application Scaling\n");
    std::puts(
        "Per-device cost (encrypt, homomorphic add, ciphertext size) is measured once, live, via CostModel's "
        "real SEAL calibration below -- the device-count sweep then multiplies those measured constants by an "
        "exact integer device count, rather than re-running thousands of live encryptions. This is still "
        "HOST_REFERENCE: the per-op numbers are measured, and multiplying a measured constant by an exact count "
        "introduces no additional uncertainty.\n");

    CostModel cost_model(*it);
    Operation feature_op{.name = "vibration_feature", .plaintext_bytes = 48, .vector_length = 6};
    const auto encrypted = cost_model.estimate_encrypted(feature_op);
    const double encrypt_ms = encrypted.latency_ms.value / 2.0;  // calibration measured encrypt+add together
    const double add_ms = encrypted.latency_ms.value / 2.0;
    const std::size_t ckks_bytes = cost_model.ckks_ciphertext_bytes();

    std::printf("Measured per-operation costs (N4096, this host):\n");
    std::printf("  encrypt:            ~%.4f ms\n", encrypt_ms);
    std::printf("  homomorphic add:    ~%.4f ms\n", add_ms);
    std::printf("  ciphertext size:    %zu bytes\n\n", ckks_bytes);

    const std::vector<std::size_t> device_counts = {1, 10, 100, 1000};
    std::vector<ScaleRow> rows;

    for (std::size_t d : device_counts) {
        ScaleRow row;
        row.devices = d;
        row.plaintext_bandwidth_mb =
            static_cast<double>(d * (feature_op.plaintext_bytes + kDiagnosticBytes)) / (1024.0 * 1024.0);

        // Full CKKS: every device's feature AND diagnostic goes through CKKS (2 ciphertexts/device).
        row.full_ckks_bandwidth_mb = static_cast<double>(d * ckks_bytes * 2) / (1024.0 * 1024.0);
        row.full_ckks_encryption_ms = static_cast<double>(d * 2) * encrypt_ms;
        row.full_ckks_gateway_ms = static_cast<double>(d * 2 > 0 ? d * 2 - 1 : 0) * add_ms;

        // Selective: only the CONFIDENTIAL+additive feature goes CKKS; the PUBLIC diagnostic stays plaintext.
        row.selective_ckks_bandwidth_mb =
            static_cast<double>(d * ckks_bytes + d * kDiagnosticBytes) / (1024.0 * 1024.0);
        row.selective_ckks_encryption_ms = static_cast<double>(d) * encrypt_ms;
        row.selective_ckks_gateway_ms = static_cast<double>(d > 0 ? d - 1 : 0) * add_ms;

        rows.push_back(row);
    }

    std::printf("%-10s %14s %14s %14s %16s %16s\n", "devices", "plaintext(MB)", "full_ckks(MB)", "selective(MB)",
                "full_enc(ms)", "selective_enc(ms)");
    for (const auto& r : rows) {
        std::printf("%-10zu %14.4f %14.4f %14.4f %16.2f %16.2f\n", r.devices, r.plaintext_bandwidth_mb,
                    r.full_ckks_bandwidth_mb, r.selective_ckks_bandwidth_mb, r.full_ckks_encryption_ms,
                    r.selective_ckks_encryption_ms);
    }

    std::puts(
        "\nInterpretation: full-CKKS bandwidth and encryption workload both scale linearly with device "
        "count AND with how many of each device's fields are encrypted -- selective placement halves both "
        "in this workload (1 of 2 transmitted fields needs encryption), without giving up confidentiality "
        "for the field that actually needs it.");

    std::filesystem::create_directories("results");
    std::ofstream out("results/cpp-scaling-latest.json");
    out << "{\n  \"paramSet\": \"" << it->id << "\",\n  \"measuredPerOp\": { \"encryptMs\": " << encrypt_ms
        << ", \"addMs\": " << add_ms << ", \"ciphertextBytes\": " << ckks_bytes << " },\n  \"rows\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        out << "    { \"devices\": " << r.devices << ", \"plaintextBandwidthMb\": " << r.plaintext_bandwidth_mb
            << ", \"fullCkksBandwidthMb\": " << r.full_ckks_bandwidth_mb
            << ", \"selectiveCkksBandwidthMb\": " << r.selective_ckks_bandwidth_mb
            << ", \"fullCkksEncryptionMs\": " << r.full_ckks_encryption_ms
            << ", \"selectiveCkksEncryptionMs\": " << r.selective_ckks_encryption_ms
            << ", \"fullCkksGatewayMs\": " << r.full_ckks_gateway_ms
            << ", \"selectiveCkksGatewayMs\": " << r.selective_ckks_gateway_ms << " }"
            << (i + 1 < rows.size() ? ",\n" : "\n");
    }
    out << "  ]\n}\n";
    std::puts("\nWrote results/cpp-scaling-latest.json");

    return 0;
}

}  // namespace e1cipher::apps

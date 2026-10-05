#include "inspect.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include "e1cipher/crypto/ckks_backend.hpp"
#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/crypto/plaintext_backend.hpp"
#include "e1cipher/fusion/feature_vector.hpp"
#include "e1cipher/platform/host_platform.hpp"
#include "e1cipher/policy/classification.hpp"
#include "e1cipher/policy/scheduler.hpp"
#include "e1cipher/telemetry/generator.hpp"

namespace e1cipher::apps {

namespace {

const char* scenario_name(telemetry::Scenario s) {
    switch (s) {
        case telemetry::Scenario::Normal:
            return "normal";
        case telemetry::Scenario::BatteryStress:
            return "battery_stress";
        case telemetry::Scenario::NetworkDegradation:
            return "network_degradation";
        case telemetry::Scenario::SensorAnomaly:
            return "sensor_anomaly";
        case telemetry::Scenario::InfraAnomaly:
            return "infra_anomaly";
    }
    return "unknown";
}

void print_rule() { std::puts("========================================="); }

void run_policy_example(std::string_view data_type, const telemetry::TelemetrySample& sample,
                        policy::MissionPriority priority, double latency_budget_ms, bool edge_available,
                        policy::CryptoAvailability crypto_avail) {
    auto baseline = policy::lookup_baseline(data_type);
    policy::SchedulerInput in;
    in.data_type = data_type;
    in.baseline_class = baseline.baseline;
    in.baseline_rationale = baseline.rationale;
    in.battery_pct = sample.battery_pct;
    in.network_mbps = sample.network_mbps;
    in.mission_priority = priority;
    in.latency_budget_ms = latency_budget_ms;
    in.estimated_encrypted_cost_ms = 1.95;
    in.edge_node_available = edge_available;
    in.crypto_availability = crypto_avail;

    auto decision = policy::decide_placement(in);
    std::printf("%-26s %s\n", std::string(data_type).c_str(), std::string(to_string(decision.placement)).c_str());
    for (const auto& line : decision.explanation) {
        std::printf("    %s\n", line.c_str());
    }
}

}  // namespace

int run_inspect(const InspectOptions& opts) {
    const auto host = platform::capture_host_info();

    print_rule();
    std::puts("E1-CIPHER");
    std::puts("CONFIDENTIAL PHYSICAL AI");
    print_rule();
    std::printf("Target:\nHOST_REFERENCE (%s, %s)\n\n", host.cpu_model.c_str(), host.arch.c_str());
    std::printf("Mission:\nINFRASTRUCTURE INSPECTION (scenario: %s)\n\n", scenario_name(opts.scenario));
    std::printf("Fleet:\n%u DRONES\n", opts.drones);

    std::puts("\n-----------------------------------------");
    std::puts("LOCAL PROCESSING");
    std::puts("-----------------------------------------\n");

    telemetry::FleetTickOptions tick_opts;
    tick_opts.seed = opts.seed;
    tick_opts.fleet_size = opts.drones;
    tick_opts.tick = opts.tick;
    tick_opts.scenario = opts.scenario;
    auto samples = telemetry::generate_fleet_tick(tick_opts);

    std::puts("RGB                    LOCAL_ONLY");
    std::puts("LiDAR                  LOCAL_ONLY");
    std::puts("IMU                    LOCAL_ONLY");
    std::puts("FEATURE VECTOR         CONFIDENTIAL\n");

    std::puts("-----------------------------------------");
    std::puts("SECURITY POLICY");
    std::puts("-----------------------------------------\n");

    const auto& representative = samples.front();
    run_policy_example("camera_frame", representative, policy::MissionPriority::Elevated, 20, true,
                       policy::CryptoAvailability::Available);
    std::puts("");
    run_policy_example("object_embedding", representative, policy::MissionPriority::Elevated, 200, true,
                       policy::CryptoAvailability::Available);
    std::puts("");
    run_policy_example("battery_telemetry", representative, policy::MissionPriority::Routine, 1000, true,
                       policy::CryptoAvailability::Available);

    std::puts("\n-----------------------------------------");
    std::puts("CKKS");
    std::puts("-----------------------------------------\n");

    const auto& sets = crypto::ckks_param_sets();
    const auto it = std::find_if(sets.begin(), sets.end(), [&](const auto& p) { return p.id == "N4096"; });
    if (it == sets.end()) {
        std::puts("CKKS parameter set N4096 not found -- aborting.");
        return 1;
    }
    std::printf("Polynomial degree:\n%zu\n\n", it->poly_modulus_degree);
    std::printf("Coefficient modulus chain (bits):\n[");
    for (std::size_t i = 0; i < it->coeff_modulus_bits.size(); ++i) {
        std::printf("%d%s", it->coeff_modulus_bits[i], i + 1 < it->coeff_modulus_bits.size() ? ", " : "");
    }
    std::printf("]\n");

    auto ckks = crypto::CkksBackend::create(*it);
    if (!ckks) {
        std::puts("Failed to construct CKKS backend -- aborting.");
        return 1;
    }

    std::puts("\n-----------------------------------------");
    std::puts("FLEET AGGREGATION");
    std::puts("-----------------------------------------\n");

    crypto::PlaintextBackend plaintext_backend;
    std::vector<fusion::FeatureVector> features;
    features.reserve(samples.size());
    for (const auto& s : samples) features.push_back(fusion::extract_feature_vector(s));

    // Plaintext path.
    crypto::EncryptedVector plain_acc = plaintext_backend.encrypt(features[0]);
    std::size_t plaintext_bytes = plain_acc.byte_length();
    for (std::size_t i = 1; i < features.size(); ++i) {
        plain_acc = plaintext_backend.add(plain_acc, plaintext_backend.encrypt(features[i]));
    }
    const auto plaintext_mean = plaintext_backend.decrypt(plain_acc);

    // CKKS path.
    std::vector<crypto::EncryptedVector> ciphers;
    ciphers.reserve(features.size());
    for (const auto& f : features) ciphers.push_back(ckks->encrypt(f));
    crypto::EncryptedVector cipher_acc = ciphers[0];
    for (std::size_t i = 1; i < ciphers.size(); ++i) cipher_acc = ckks->add(cipher_acc, ciphers[i]);
    const auto ckks_sum = ckks->decrypt(cipher_acc);

    double max_err = 0.0;
    for (std::size_t i = 0; i < fusion::kFeatureCount; ++i) {
        const double ckks_mean_i = ckks_sum.values[i] / static_cast<double>(features.size());
        const double plain_mean_i = plaintext_mean.values[i] / static_cast<double>(features.size());
        max_err = std::max(max_err, std::abs(ckks_mean_i - plain_mean_i));
    }

    const std::size_t total_plaintext_bytes = plaintext_bytes * features.size();
    const std::size_t total_ckks_bytes = ciphers[0].byte_length() * ciphers.size();

    std::printf("Plaintext bytes transmitted:\n%zu\n\n", total_plaintext_bytes);
    std::printf("CKKS ciphertext bytes transmitted:\n%zu\n\n", total_ckks_bytes);
    std::printf("Ciphertext expansion:\n%.1fx\n\n",
                static_cast<double>(total_ckks_bytes) / static_cast<double>(total_plaintext_bytes));
    std::printf("Mean accuracy (max abs error):\n%.3e\n", max_err);

    std::puts("\n-----------------------------------------");
    std::puts("SECURITY");
    std::puts("-----------------------------------------\n");
    std::puts("Raw telemetry transmitted:\n0\n");
    std::puts("Sensitive telemetry transmitted:\nENCRYPTED\n");

    std::puts("-----------------------------------------");
    std::puts("E1 STATUS");
    std::puts("-----------------------------------------\n");
    std::puts("E1 hardware:\nNOT CONNECTED\n");
    std::puts("E1 validation:\nPENDING HARDWARE");

    print_rule();
    return 0;
}

}  // namespace e1cipher::apps

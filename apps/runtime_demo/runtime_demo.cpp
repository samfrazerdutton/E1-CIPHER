#include "runtime_demo.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/fusion/feature_vector.hpp"
#include "e1cipher/runtime/runtime.hpp"
#include "e1cipher/runtime/workload.hpp"

namespace e1cipher::apps {

using namespace e1cipher::runtime;

namespace {

void rule() { std::puts("=================================================="); }

void sub_rule() { std::puts("--------------------------------------------------"); }

}  // namespace

int run_runtime_demo() {
    const auto& sets = crypto::ckks_param_sets();
    const auto it = std::find_if(sets.begin(), sets.end(), [](const auto& p) { return p.id == "N4096"; });
    if (it == sets.end()) {
        std::puts("CKKS parameter set N4096 not found -- aborting.");
        return 1;
    }

    rule();
    std::puts("E1-CIPHER CONFIDENTIAL EDGE RUNTIME");
    rule();

    Workload workload = industrial_inspection_workload();
    std::printf("\nWorkload:\n%s\n", workload.name.c_str());
    std::puts("\nFleet:\n100 autonomous devices (framing only -- see `e1cipher experiment scaling` for a measured");
    std::puts("device-count sweep; this demo analyzes one device's operation set).");
    std::puts("\nSecurity policy:\nCONFIDENTIAL physical telemetry\n");

    Runtime rt(*it);
    SecurityPolicy policy;
    policy.crypto_backend_available = true;
    policy.accelerator_available = true;
    HardwareProfile hw;
    hw.network_mbps = 10;
    PerformanceBudget budget;
    budget.latency_budget_ms = 500;

    sub_rule();
    std::puts("WORKLOAD ANALYSIS");
    sub_rule();

    std::size_t naive_all_encrypted_bytes = 0;
    std::size_t actual_transmitted_bytes = 0;
    int accelerated_count = 0;
    int encrypted_count = 0;
    int local_count = 0;
    int remote_count = 0;
    int blocked_count = 0;

    for (const auto& op : workload.operations) {
        auto decision = rt.decide(op, policy, hw, budget);
        std::printf("\n%s\n", op.name.c_str());
        std::printf("  classification: %s\n", std::string(to_string(op.sensitivity)).c_str());
        std::printf("  placement: %s\n", std::string(to_string(decision.placement)).c_str());
        std::printf("  reason: %s\n", decision.explanation.back().c_str());

        if (op.sensitivity != DataSensitivity::Secret && op.sensitivity != DataSensitivity::Public) {
            naive_all_encrypted_bytes +=
                rt.cost_model().estimate_encrypted(op).bandwidth_bytes.value > 0
                    ? static_cast<std::size_t>(rt.cost_model().estimate_encrypted(op).bandwidth_bytes.value)
                    : 0;
        }

        switch (decision.placement) {
            case Placement::Local:
                ++local_count;
                break;
            case Placement::Encrypted:
                ++encrypted_count;
                actual_transmitted_bytes += static_cast<std::size_t>(decision.cost.bandwidth_bytes.value);
                break;
            case Placement::Remote:
                ++remote_count;
                actual_transmitted_bytes += static_cast<std::size_t>(decision.cost.bandwidth_bytes.value);
                break;
            case Placement::Accelerated:
                ++accelerated_count;
                actual_transmitted_bytes += static_cast<std::size_t>(decision.cost.bandwidth_bytes.value);
                break;
            case Placement::Blocked:
                ++blocked_count;
                break;
        }
    }

    std::printf("\n");
    sub_rule();
    std::puts("ARCHITECTURE");
    sub_rule();
    std::printf("\nLocal compute (never transmitted):\n%d operation(s)\n", local_count);
    std::printf("\nEncrypted compute (CKKS, %s):\n%d operation(s)\n", std::string(it->id).c_str(), encrypted_count);
    std::printf("\nAccelerator candidate (NTT, host-reference proxy):\n%d operation(s)\n", accelerated_count);
    std::printf("\nRemote plaintext:\n%d operation(s)\n", remote_count);
    std::printf("\nBlocked (fail-closed):\n%d operation(s)\n", blocked_count);

    std::printf("\n");
    sub_rule();
    std::puts("MEASURED COST (HOST_REFERENCE unless noted)");
    sub_rule();

    const auto enc = rt.cost_model().estimate_encrypted(Operation{});
    std::printf("\nCKKS encrypt+add (one feature vector, %zu-dim):\n%.3f ms\n", fusion::kFeatureCount,
                enc.latency_ms.value);
    std::printf("\nCiphertext size:\n%zu bytes\n", rt.cost_model().ckks_ciphertext_bytes());
    std::printf("\nCiphertext expansion vs. plaintext feature vector:\n%.0fx\n",
                static_cast<double>(rt.cost_model().ckks_ciphertext_bytes()) /
                    static_cast<double>(fusion::kFeatureCount * sizeof(double)));

    std::printf("\n");
    sub_rule();
    std::puts("DECISION");
    sub_rule();

    std::puts("\nSelective encrypted execution:\nENABLED");
    const double naive_mb = static_cast<double>(naive_all_encrypted_bytes) / (1024.0 * 1024.0);
    const double actual_mb = static_cast<double>(actual_transmitted_bytes) / (1024.0 * 1024.0);
    std::printf(
        "\nBandwidth if every non-SECRET/non-PUBLIC operation had been routed through ENCRYPTED (naive "
        "baseline):\n%.3f "
        "MB\n",
        naive_mb);
    std::printf("\nBandwidth actually transmitted under selective placement:\n%.3f MB\n", actual_mb);
    std::printf("\nOperations eligible for spatial acceleration:\n%d of %zu\n", accelerated_count,
                workload.operations.size());
    std::puts(
        "\nNote: the gap between the naive and actual bandwidth numbers above comes entirely from NOT encrypting "
        "PUBLIC/RESTRICTED data -- not from any crypto optimization. See docs/technical-deep-dive.md.");

    rule();
    return 0;
}

}  // namespace e1cipher::apps

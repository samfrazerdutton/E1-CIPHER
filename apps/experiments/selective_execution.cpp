#include "selective_execution.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/runtime/runtime.hpp"
#include "e1cipher/runtime/workload.hpp"

namespace e1cipher::apps {

using namespace e1cipher::runtime;

namespace {

struct ModeResult {
    std::string name;
    double latency_ms = 0;
    std::size_t bandwidth_bytes = 0;
    std::size_t security_exposed_bytes = 0;  ///< Confidential+ plaintext bytes that left the device unencrypted.
};

bool is_vector_shaped(const Operation& op) { return op.vector_length > 0; }

}  // namespace

int run_selective_execution_experiment() {
    const auto& sets = crypto::ckks_param_sets();
    const auto it = std::find_if(sets.begin(), sets.end(), [](const auto& p) { return p.id == "N4096"; });
    if (it == sets.end()) {
        std::puts("CKKS parameter set N4096 not found -- aborting.");
        return 1;
    }

    std::puts("EXPERIMENT: Selective Confidential Execution\n");
    std::puts(
        "Raw, unstructured sensor streams (camera_frame) are excluded from all three modes below -- this "
        "reference CKKS backend operates on fixed-size feature vectors, and feature extraction happens before any "
        "encryption decision in every mode. What varies between modes is whether the DERIVED, vector-shaped "
        "operations are sent plaintext, encrypted, or selectively routed.\n");

    Workload workload = industrial_inspection_workload();
    Runtime rt(*it);
    HardwareProfile hw;
    hw.network_mbps = 10;
    PerformanceBudget budget;
    budget.latency_budget_ms = 500;

    std::vector<Operation> eligible;
    for (const auto& op : workload.operations) {
        if (is_vector_shaped(op)) eligible.push_back(op);
    }

    ModeResult mode_a{"A: Everything plaintext"};
    ModeResult mode_b{"B: Everything encrypted"};
    ModeResult mode_c{"C: Selective (runtime::Runtime::decide)"};

    SecurityPolicy policy_c;
    policy_c.crypto_backend_available = true;
    policy_c.accelerator_available = true;
    policy_c.max_sensitivity_for_remote_plaintext = DataSensitivity::Public;

    for (const auto& op : eligible) {
        // --- Mode A: ignore sensitivity, send everything plaintext remote. ---
        {
            const auto cost = rt.cost_model().estimate_remote(op, hw);
            mode_a.latency_ms += cost.latency_ms.value;
            mode_a.bandwidth_bytes += op.plaintext_bytes;
            if (op.sensitivity >= DataSensitivity::Confidential) mode_a.security_exposed_bytes += op.plaintext_bytes;
        }
        // --- Mode B: encrypt everything, regardless of sensitivity. ---
        {
            const auto cost = rt.cost_model().estimate_encrypted(op);
            mode_b.latency_ms += cost.latency_ms.value;
            mode_b.bandwidth_bytes += static_cast<std::size_t>(cost.bandwidth_bytes.value);
            // Nothing exposed -- encrypted, even the Public ops (wastefully).
        }
        // --- Mode C: let the runtime decide. ---
        {
            auto decision = rt.decide(op, policy_c, hw, budget);
            mode_c.latency_ms += decision.cost.latency_ms.value;
            mode_c.bandwidth_bytes += static_cast<std::size_t>(decision.cost.bandwidth_bytes.value);
            if (op.sensitivity >= DataSensitivity::Confidential && decision.placement == Placement::Remote) {
                mode_c.security_exposed_bytes += op.plaintext_bytes;  // should never happen -- fail-closed guards this
            }
        }
    }

    const auto print_mode = [](const ModeResult& m) {
        std::printf("%s\n", m.name.c_str());
        std::printf("  latency:          %.4f ms\n", m.latency_ms);
        std::printf("  bandwidth:        %zu bytes\n", m.bandwidth_bytes);
        std::printf("  security exposed: %zu bytes (CONFIDENTIAL+ data transmitted in the clear)\n\n",
                    m.security_exposed_bytes);
    };
    print_mode(mode_a);
    print_mode(mode_b);
    print_mode(mode_c);

    std::puts("Interpretation:");
    std::printf("  Mode A is %.0fx cheaper in bandwidth than Mode B, but exposes %zu bytes of confidential data.\n",
                static_cast<double>(mode_b.bandwidth_bytes) /
                    static_cast<double>(std::max<std::size_t>(mode_a.bandwidth_bytes, 1)),
                mode_a.security_exposed_bytes);
    std::printf("  Mode C matches Mode B's zero exposure (%zu bytes) while using %.1f%% of Mode B's bandwidth.\n",
                mode_c.security_exposed_bytes,
                100.0 * static_cast<double>(mode_c.bandwidth_bytes) /
                    static_cast<double>(std::max<std::size_t>(mode_b.bandwidth_bytes, 1)));

    std::filesystem::create_directories("results");
    std::ofstream out("results/cpp-selective-execution-latest.json");
    out << "{\n  \"paramSet\": \"" << it->id << "\",\n  \"modes\": [\n";
    const ModeResult* modes[] = {&mode_a, &mode_b, &mode_c};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& m = *modes[i];
        out << "    { \"name\": \"" << m.name << "\", \"latencyMs\": " << m.latency_ms
            << ", \"bandwidthBytes\": " << m.bandwidth_bytes
            << ", \"securityExposedBytes\": " << m.security_exposed_bytes << " }" << (i < 2 ? ",\n" : "\n");
    }
    out << "  ]\n}\n";
    std::puts("\nWrote results/cpp-selective-execution-latest.json");

    return 0;
}

}  // namespace e1cipher::apps

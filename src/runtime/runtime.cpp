#include "e1cipher/runtime/runtime.hpp"

#include <format>
#include <stdexcept>
#include <string>

namespace e1cipher::runtime {

Runtime::Runtime(const crypto::CkksParamSet& param_set) : cost_model_(param_set) {}

PlacementDecision Runtime::decide(const Operation& op, const SecurityPolicy& policy, const HardwareProfile& hw,
                                  const PerformanceBudget& budget) const {
    PlacementDecision d;
    d.explanation.push_back(std::format("Operation '{}': kind={}, sensitivity={}, {} bytes, additive={}, linear={}",
                                        op.name, to_string(op.kind), to_string(op.sensitivity), op.plaintext_bytes,
                                        op.additive, op.linear));

    // --- fail closed, rule 1: SECRET never leaves the device. ---
    if (op.sensitivity == DataSensitivity::Secret) {
        d.explanation.push_back("SECRET data never leaves the device regardless of resources or budget.");
        d.placement = Placement::Local;
        d.cost = cost_model_.estimate_local(op);
        return d;
    }

    const bool needs_confidentiality = op.sensitivity >= DataSensitivity::Confidential;

    // --- fail closed, rule 2: confidentiality required, no backend -> blocked, never silently plaintext. ---
    if (needs_confidentiality && !policy.crypto_backend_available) {
        d.explanation.push_back(
            "Sensitivity requires encryption but no crypto backend is available; failing closed rather than "
            "transmitting unencrypted.");
        d.placement = Placement::Blocked;
        return d;
    }

    const bool remote_plaintext_allowed = op.sensitivity <= policy.max_sensitivity_for_remote_plaintext;
    const bool accelerator_eligible =
        policy.accelerator_available && policy.crypto_backend_available && op.additive && op.linear;

    // Battery-critical deferral (ports lib/policy/scheduler.ts's same
    // rule): below kCriticalBatteryPct, hold non-critical-priority work
    // locally rather than paying encryption/transmission cost right now.
    // additive ops are the one exception -- their value only exists once
    // combined across devices, so deferring them locally forever would
    // make them pointless; they still get encrypted/accelerated below,
    // just not deferred on battery grounds.
    constexpr double kCriticalBatteryPct = 15.0;
    const bool battery_critical = hw.battery_pct < kCriticalBatteryPct && budget.priority != MissionPriority::Critical;
    if (battery_critical && !op.additive) {
        d.explanation.push_back(
            std::format("Battery below critical threshold ({:.0f}% < {:.0f}%) and mission priority is not "
                        "Critical; deferring non-essential work locally.",
                        hw.battery_pct, kCriticalBatteryPct));
        d.placement = Placement::Local;
        d.cost = cost_model_.estimate_local(op);
        return d;
    }

    if (needs_confidentiality) {
        const CostEstimate encrypted_cost = cost_model_.estimate_encrypted(op);
        d.alternatives_considered.push_back(std::format(
            "LOCAL: rejected -- {}", op.additive ? "this operation's value only exists when combined across "
                                                   "devices; keeping it local defeats its purpose"
                                                 : "data is sensitive and its intended use requires sharing it"));
        d.alternatives_considered.push_back(
            std::format("REMOTE (plaintext): rejected -- sensitivity ({}) exceeds policy threshold ({})",
                        to_string(op.sensitivity), to_string(policy.max_sensitivity_for_remote_plaintext)));

        if (accelerator_eligible) {
            d.alternatives_considered.push_back(
                std::format("ENCRYPTED (no acceleration): available (est. {:.3f} ms) but this op is additive+linear, a "
                            "candidate-kernel acceleration path exists, and one is configured as available",
                            encrypted_cost.latency_ms.value));
            d.explanation.push_back(
                "Sensitivity requires encryption; this operation is additive and linear, and an accelerator "
                "candidate backend is available -- routed to ACCELERATED.");
            d.placement = Placement::Accelerated;
            d.cost = cost_model_.estimate_accelerated(op);
        } else {
            if (policy.accelerator_available && op.additive && op.linear) {
                d.alternatives_considered.push_back(
                    "ACCELERATED: operation is additive+linear (eligible in principle) but no accelerator "
                    "candidate backend is configured as available for this decision");
            }
            d.explanation.push_back(
                std::format("Sensitivity requires encryption; routed to ENCRYPTED (est. {:.3f} ms, {:.0f} bytes).",
                            encrypted_cost.latency_ms.value, encrypted_cost.bandwidth_bytes.value));
            d.placement = Placement::Encrypted;
            d.cost = encrypted_cost;
        }
        return d;
    }

    // Public / Restricted, no confidentiality requirement.
    if (remote_plaintext_allowed) {
        const CostEstimate remote_cost = cost_model_.estimate_remote(op, hw);
        if (remote_cost.latency_ms.value <= budget.latency_budget_ms) {
            d.alternatives_considered.push_back(std::format(
                "LOCAL: available but unnecessary -- data is {} and fits the {:.0f} ms latency budget remotely",
                to_string(op.sensitivity), budget.latency_budget_ms));
            d.explanation.push_back(std::format(
                "Low-sensitivity data within latency budget; routed to REMOTE (est. {:.3f} ms, {} bytes, ESTIMATED).",
                remote_cost.latency_ms.value, op.plaintext_bytes));
            d.placement = Placement::Remote;
            d.cost = remote_cost;
            return d;
        }
        d.alternatives_considered.push_back(
            std::format("REMOTE: rejected -- estimated {:.3f} ms exceeds the {:.0f} ms latency budget",
                        remote_cost.latency_ms.value, budget.latency_budget_ms));
    } else {
        d.alternatives_considered.push_back(
            std::format("REMOTE: rejected -- sensitivity ({}) exceeds policy threshold ({}) for plaintext transmission",
                        to_string(op.sensitivity), to_string(policy.max_sensitivity_for_remote_plaintext)));
    }

    d.explanation.push_back("Routed to LOCAL (not transmitted).");
    d.placement = Placement::Local;
    d.cost = cost_model_.estimate_local(op);
    return d;
}

void Runtime::execute(const Operation& op, const PlacementDecision& decision) {
    switch (decision.placement) {
        case Placement::Blocked:
            throw std::runtime_error("Runtime::execute: refusing to execute a BLOCKED operation ('" + op.name + "')");
        case Placement::Accelerated: {
            const std::size_t n = op.vector_length > 0 ? op.vector_length : 16;
            auto buf = host_backend_.allocate_buffer(n);
            host_backend_.submit_kernel(KernelId::Ntt, buf);
            host_backend_.synchronize();
            return;
        }
        case Placement::Local:
        case Placement::Encrypted:
        case Placement::Remote:
            // This reference runtime's CostModel already performed the
            // representative real work for these placements during
            // decide(); execute() here is a deliberate no-op rather than
            // doing the same work twice under a different name.
            return;
    }
}

}  // namespace e1cipher::runtime

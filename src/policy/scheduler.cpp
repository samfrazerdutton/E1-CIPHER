#include "e1cipher/policy/scheduler.hpp"

#include <format>

namespace e1cipher::policy {

namespace {

std::string_view mission_priority_str(MissionPriority p) {
    switch (p) {
        case MissionPriority::Routine:
            return "routine";
        case MissionPriority::Elevated:
            return "elevated";
        case MissionPriority::Critical:
            return "critical";
    }
    return "routine";
}

bool needs_encryption(Classification c) {
    return c == Classification::Confidential || c == Classification::Aggregatable;
}

}  // namespace

PlacementDecision decide_placement(const SchedulerInput& in) {
    const auto& t = in.thresholds;
    PlacementDecision d;
    d.explanation.push_back(std::format("Data type '{}' classified {}: {}", in.data_type, to_string(in.baseline_class),
                                        in.baseline_rationale));
    d.explanation.push_back(std::format("Battery remaining: {:.1f}%.", in.battery_pct));
    d.explanation.push_back(std::format("Network available: {:.2f} Mbps.", in.network_mbps));
    d.explanation.push_back(std::format("Mission priority: {}.", mission_priority_str(in.mission_priority)));

    // --- fail closed, rule 1: never classified -> never placed. ---
    if (in.baseline_class == Classification::Unknown) {
        d.explanation.push_back(
            "Classification is UNKNOWN (no catalog entry); failing closed. Data is discarded, not transmitted.");
        d.placement = Placement::Blocked;
        d.transmitted = false;
        d.encrypted = false;
        return d;
    }

    if (in.baseline_class == Classification::LocalOnly) {
        d.explanation.push_back("LOCAL_ONLY data never leaves the device regardless of battery/network/mission state.");
        d.placement = Placement::LocalPlaintext;
        d.transmitted = false;
        d.encrypted = false;
        return d;
    }

    const bool network_ok = in.network_mbps >= t.min_network_mbps_to_transmit;
    const bool need_enc = needs_encryption(in.baseline_class);
    const bool battery_critical = in.battery_pct < t.critical_battery_pct;

    // --- fail closed, rule 2: needs encryption, no crypto backend -> blocked, not silently sent plaintext. ---
    if (need_enc && in.crypto_availability == CryptoAvailability::Unavailable) {
        d.explanation.push_back(
            "This data requires encryption but no crypto backend is available; failing closed rather than "
            "transmitting it unencrypted.");
        d.placement = Placement::Blocked;
        d.transmitted = false;
        d.encrypted = false;
        return d;
    }

    if (!network_ok) {
        d.explanation.push_back(std::format("Network below {:.2f} Mbps transmit threshold; holding data locally.",
                                            t.min_network_mbps_to_transmit));
        d.placement = need_enc ? Placement::LocalEncrypted : Placement::LocalPlaintext;
        d.transmitted = false;
        d.encrypted = need_enc;
        return d;
    }

    if (need_enc) {
        d.explanation.push_back(
            std::format("Encrypted path estimated cost: {:.2f} ms.", in.estimated_encrypted_cost_ms));
    }

    if (battery_critical && in.mission_priority != MissionPriority::Critical) {
        d.explanation.push_back(std::format(
            "Battery below critical threshold ({:.0f}%) and mission priority is not critical; deferring non-essential "
            "{}transmission.",
            t.critical_battery_pct, need_enc ? "encryption and " : ""));
        d.placement = need_enc ? Placement::LocalEncrypted : Placement::LocalPlaintext;
        d.transmitted = false;
        d.encrypted = need_enc;
        return d;
    }

    const bool tight_latency = in.latency_budget_ms <= t.low_latency_budget_ms;

    if (!need_enc) {
        d.placement = (tight_latency && in.edge_node_available) ? Placement::EdgePlaintext : Placement::CloudPlaintext;
        d.explanation.push_back(
            std::format("Low-sensitivity data permitted to transmit unencrypted; routed {}.", to_string(d.placement)));
        d.transmitted = true;
        d.encrypted = false;
        return d;
    }

    if (tight_latency) {
        if (in.edge_node_available) {
            d.explanation.push_back(std::format(
                "Latency budget ({:.0f} ms) is tight and an edge node is available; edge encrypted execution selected.",
                in.latency_budget_ms));
            d.placement = Placement::EdgeEncrypted;
        } else {
            d.explanation.push_back(
                std::format("Latency budget ({:.0f} ms) is tight and no edge node is available; local encrypted "
                            "execution selected.",
                            in.latency_budget_ms));
            d.placement = Placement::LocalEncrypted;
        }
        d.transmitted = (d.placement == Placement::EdgeEncrypted);
        d.encrypted = true;
        return d;
    }

    d.explanation.push_back("Latency budget permits off-device aggregation; cloud encrypted execution selected.");
    d.placement = Placement::CloudEncrypted;
    d.transmitted = true;
    d.encrypted = true;
    return d;
}

}  // namespace e1cipher::policy

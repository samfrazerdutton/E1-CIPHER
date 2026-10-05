#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "e1cipher/policy/classification.hpp"

namespace e1cipher::policy {

/// Compute placement the adaptive scheduler can choose. `Blocked` is a
/// real, reachable state (see "fail closed" below) — not an error path
/// bolted on afterward.
enum class Placement {
    LocalPlaintext,
    LocalEncrypted,
    EdgePlaintext,
    EdgeEncrypted,
    CloudPlaintext,
    CloudEncrypted,
    Blocked,
};

constexpr std::string_view to_string(Placement p) noexcept {
    switch (p) {
        case Placement::LocalPlaintext:
            return "LOCAL_PLAINTEXT";
        case Placement::LocalEncrypted:
            return "LOCAL_ENCRYPTED";
        case Placement::EdgePlaintext:
            return "EDGE_PLAINTEXT";
        case Placement::EdgeEncrypted:
            return "EDGE_ENCRYPTED";
        case Placement::CloudPlaintext:
            return "CLOUD_PLAINTEXT";
        case Placement::CloudEncrypted:
            return "CLOUD_ENCRYPTED";
        case Placement::Blocked:
            return "BLOCKED";
    }
    return "BLOCKED";
}

enum class MissionPriority { Routine, Elevated, Critical };

struct SchedulerThresholds {
    double min_network_mbps_to_transmit = 0.3;
    double critical_battery_pct = 15.0;
    double low_latency_budget_ms = 50.0;
};

inline constexpr SchedulerThresholds kDefaultThresholds{};

/// Whether a crypto backend capable of CONFIDENTIAL/AGGREGATABLE work is
/// actually available right now. The scheduler takes this as an input
/// rather than assuming one always exists — see `fail closed` rule 4.
enum class CryptoAvailability { Available, Unavailable };

struct SchedulerInput {
    std::string_view data_type;
    Classification baseline_class = Classification::Unknown;
    std::string_view baseline_rationale;
    double battery_pct = 0.0;
    double network_mbps = 0.0;
    MissionPriority mission_priority = MissionPriority::Routine;
    double latency_budget_ms = 0.0;
    double estimated_encrypted_cost_ms = 0.0;
    bool edge_node_available = false;
    CryptoAvailability crypto_availability = CryptoAvailability::Unavailable;
    SchedulerThresholds thresholds = kDefaultThresholds;
};

struct PlacementDecision {
    Placement placement = Placement::Blocked;
    bool transmitted = false;
    bool encrypted = false;
    std::vector<std::string> explanation;
};

/// Deterministic, rule-based policy decision (never a black box) —
/// explainable by construction. Ports lib/policy/scheduler.ts's
/// decidePlacement, with fail-closed behavior added (see
/// docs/architecture-assessment.md):
///   - Classification::Unknown                         -> Blocked
///   - needs encryption but crypto_availability==Unavailable -> Blocked
/// Neither case existed as a reachable "deny" path in the TS version.
[[nodiscard]] PlacementDecision decide_placement(const SchedulerInput& input);

}  // namespace e1cipher::policy

#pragma once
#include <string_view>

namespace e1cipher::policy {

/// Data-sensitivity classification. Mirrors lib/policy/classification.ts's
/// SecurityClass, with one addition the TS version did not have:
/// `Unknown` — an explicit, first-class "this data was never classified"
/// state, so the scheduler can fail closed on it (see
/// docs/architecture-assessment.md: the TS scheduler always resolved to
/// *some* placement; that was a real gap this port fixes).
enum class Classification {
    Unknown,       ///< Not classified. The scheduler MUST block, never guess.
    LocalOnly,     ///< Never leaves the device, regardless of resources.
    Plaintext,     ///< Low sensitivity; may transmit unencrypted.
    Confidential,  ///< Must be encrypted before it leaves the device.
    Aggregatable,  ///< Confidential, specifically destined for homomorphic fleet aggregation.
};

constexpr std::string_view to_string(Classification c) noexcept {
    switch (c) {
        case Classification::Unknown:
            return "UNKNOWN";
        case Classification::LocalOnly:
            return "LOCAL_ONLY";
        case Classification::Plaintext:
            return "PLAINTEXT";
        case Classification::Confidential:
            return "CONFIDENTIAL";
        case Classification::Aggregatable:
            return "AGGREGATABLE";
    }
    return "UNKNOWN";
}

/// One entry in the static baseline-classification catalog.
struct ClassificationEntry {
    std::string_view data_type;
    Classification baseline;
    std::string_view rationale;
};

/// Static baseline table — the starting point the policy scheduler then
/// adjusts for runtime conditions (see scheduler.hpp). Mirrors
/// lib/policy/classification.ts's DATA_CATALOG verbatim.
inline constexpr ClassificationEntry kDataCatalog[] = {
    {"camera_frame", Classification::LocalOnly,
     "Raw imagery is large, highly sensitive, and not needed off-device once features are extracted."},
    {"object_bounding_box", Classification::Plaintext,
     "Low-sensitivity geometric summary already stripped of raw pixels; local use only, cheap to keep plaintext."},
    {"object_embedding", Classification::Confidential,
     "Derived feature vector can re-identify what the drone observed; encrypt before it leaves the device."},
    {"fleet_aggregate_statistic", Classification::Aggregatable,
     "Only the aggregate (mean/sum across drones) is useful centrally -- CKKS lets the server compute it without "
     "seeing inputs."},
    {"battery_telemetry", Classification::Plaintext,
     "Operationally necessary, low sensitivity, needed quickly and cheaply for fleet health dashboards."},
    {"restricted_location_telemetry", Classification::Confidential,
     "GPS trace over a restricted site is sensitive on its own, independent of any other signal."},
    {"emergency_flight_control", Classification::LocalOnly,
     "Safety-critical control loop; must never depend on network or cryptographic availability."},
};

/// Looks up a data type's baseline classification. Returns a catalog entry
/// with `baseline == Classification::Unknown` (NOT a null/optional) for an
/// unrecognized type, forcing callers to handle the fail-closed case
/// explicitly rather than accidentally treat "not found" as "permitted."
[[nodiscard]] ClassificationEntry lookup_baseline(std::string_view data_type) noexcept;

}  // namespace e1cipher::policy

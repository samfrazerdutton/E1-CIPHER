#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "e1cipher/platform/result_class.hpp"

namespace e1cipher::runtime {

/// Generalizes lib/policy/classification.hpp's Classification (which was
/// drone-telemetry-specific) to any physical-AI workload. Ordered from
/// least to most sensitive on purpose -- comparisons like
/// `sensitivity >= DataSensitivity::Confidential` are meaningful.
enum class DataSensitivity {
    Public,        ///< May be transmitted and stored in the clear, anywhere.
    Restricted,    ///< Operationally sensitive but low-value if exposed (e.g. battery level).
    Confidential,  ///< Must not be exposed in the clear off-device.
    Secret,        ///< Must not leave the device in any form, plaintext or ciphertext.
};

constexpr std::string_view to_string(DataSensitivity s) noexcept {
    switch (s) {
        case DataSensitivity::Public:
            return "PUBLIC";
        case DataSensitivity::Restricted:
            return "RESTRICTED";
        case DataSensitivity::Confidential:
            return "CONFIDENTIAL";
        case DataSensitivity::Secret:
            return "SECRET";
    }
    return "UNKNOWN";
}

/// What shape of computation an Operation represents -- this is what
/// determines whether CKKS (additive/linear only, approximate) or an
/// accelerator candidate kernel (regular, data-parallel) is even
/// eligible, independent of sensitivity.
enum class OperationKind {
    SensorRead,      ///< Raw ingestion from a sensor; not itself a transform.
    FeatureExtract,  ///< Local, typically nonlinear, reduction of raw data to a compact representation.
    Aggregate,       ///< Additive combination across multiple devices/samples (CKKS-friendly).
    Transform,       ///< A nonlinear or nondeterministic operation (NOT CKKS-friendly without bootstrapping).
    Diagnostic,      ///< Low-value, low-sensitivity housekeeping data.
};

constexpr std::string_view to_string(OperationKind k) noexcept {
    switch (k) {
        case OperationKind::SensorRead:
            return "SENSOR_READ";
        case OperationKind::FeatureExtract:
            return "FEATURE_EXTRACT";
        case OperationKind::Aggregate:
            return "AGGREGATE";
        case OperationKind::Transform:
            return "TRANSFORM";
        case OperationKind::Diagnostic:
            return "DIAGNOSTIC";
    }
    return "UNKNOWN";
}

/// One unit of work the runtime must place. Deliberately NOT tied to any
/// one application (drones, robots, ...) -- see workload.hpp for the three
/// concrete application profiles built from these.
struct Operation {
    std::string name;
    OperationKind kind = OperationKind::Diagnostic;
    DataSensitivity sensitivity = DataSensitivity::Public;
    std::size_t plaintext_bytes = 0;   ///< Size of this operation's input in the clear.
    std::size_t vector_length = 0;     ///< Scalar element count (drives CKKS slot sizing); 0 if not vector-shaped.
    bool additive = false;             ///< Expressible as a homomorphic sum across devices/samples.
    bool linear = false;               ///< Expressible via CKKS add/multiply without bootstrapping.
    double accuracy_tolerance = 1e-6;  ///< Acceptable approximation error (CKKS is lossy).
};

/// Execution backend the runtime placed an Operation on. These are
/// backend choices, not network-topology choices -- "Encrypted" means
/// "ran through the CKKS backend," regardless of whether the result also
/// happens to be transmitted.
enum class Placement {
    Local,        ///< Runs on-device, in the clear. Never leaves.
    Encrypted,    ///< Runs through the CKKS CryptoBackend.
    Remote,       ///< Transmitted off-device in the clear (only ever for Public/Restricted data).
    Accelerated,  ///< Routed to an accelerator-candidate kernel (host proxy today; E1 once available).
    Blocked,      ///< Refused -- see "fail closed" in runtime.cpp.
};

constexpr std::string_view to_string(Placement p) noexcept {
    switch (p) {
        case Placement::Local:
            return "LOCAL";
        case Placement::Encrypted:
            return "ENCRYPTED";
        case Placement::Remote:
            return "REMOTE";
        case Placement::Accelerated:
            return "ACCELERATED";
        case Placement::Blocked:
            return "BLOCKED";
    }
    return "BLOCKED";
}

/// One estimated quantity, tagged with where the number actually came
/// from -- never presented without this tag. See
/// include/e1cipher/platform/result_class.hpp.
struct CostValue {
    double value = 0;
    platform::ResultClass provenance = platform::ResultClass::NotMeasured;
};

struct CostEstimate {
    CostValue latency_ms;
    CostValue bandwidth_bytes;
    CostValue memory_bytes;
};

struct PlacementDecision {
    Placement placement = Placement::Blocked;
    CostEstimate cost;
    std::string reason;
    std::vector<std::string> alternatives_considered;  ///< e.g. "REMOTE: rejected -- sensitivity exceeds policy".
    std::vector<std::string> explanation;              ///< Full human-readable decision trail, in order.
};

/// What the runtime is allowed to do with data at each sensitivity level,
/// and whether a crypto backend actually exists right now. Generalizes
/// lib/policy/scheduler.hpp's fail-closed CryptoAvailability check.
struct SecurityPolicy {
    // Default is deliberately Public only: a RESTRICTED-but-precise signal
    // (e.g. exact GPS) still shouldn't transmit in the clear by default --
    // a caller must opt in to a looser threshold, not the other way round.
    DataSensitivity max_sensitivity_for_remote_plaintext = DataSensitivity::Public;
    bool crypto_backend_available = false;
    bool accelerator_available = false;
};

struct HardwareProfile {
    double battery_pct = 100;
    double network_mbps = 10;
};

enum class MissionPriority { Routine, Elevated, Critical };

struct PerformanceBudget {
    double latency_budget_ms = 1000;
    MissionPriority priority = MissionPriority::Routine;
};

}  // namespace e1cipher::runtime

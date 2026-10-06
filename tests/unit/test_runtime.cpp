#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <algorithm>
#include <doctest/doctest.h>

#include "e1cipher/runtime/runtime.hpp"
#include "e1cipher/runtime/workload.hpp"

using namespace e1cipher::runtime;
using namespace e1cipher::crypto;

namespace {
const CkksParamSet& n4096() {
    const auto& sets = ckks_param_sets();
    auto it = std::find_if(sets.begin(), sets.end(), [](const auto& p) { return p.id == "N4096"; });
    REQUIRE(it != sets.end());
    return *it;
}
}  // namespace

TEST_CASE("SECRET data is always placed LOCAL, regardless of policy/hardware") {
    Runtime rt(n4096());
    Operation op{.name = "camera_frame",
                 .kind = OperationKind::SensorRead,
                 .sensitivity = DataSensitivity::Secret,
                 .plaintext_bytes = 1000000};
    SecurityPolicy policy{.crypto_backend_available = true, .accelerator_available = true};
    HardwareProfile hw{.network_mbps = 1000};
    PerformanceBudget budget{.latency_budget_ms = 10000};

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Local);
}

TEST_CASE("CONFIDENTIAL data with no crypto backend fails closed (BLOCKED)") {
    Runtime rt(n4096());
    Operation op{.name = "feature",
                 .kind = OperationKind::FeatureExtract,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 64};
    SecurityPolicy policy{.crypto_backend_available = false};
    HardwareProfile hw;
    PerformanceBudget budget;

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Blocked);
}

TEST_CASE("CONFIDENTIAL + additive + linear routes to ACCELERATED when an accelerator candidate is available") {
    Runtime rt(n4096());
    Operation op{.name = "fleet_stat",
                 .kind = OperationKind::Aggregate,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 48,
                 .vector_length = 6,
                 .additive = true,
                 .linear = true};
    SecurityPolicy policy{.crypto_backend_available = true, .accelerator_available = true};
    HardwareProfile hw;
    PerformanceBudget budget;

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Accelerated);
}

TEST_CASE("CONFIDENTIAL + additive + linear routes to ENCRYPTED when no accelerator candidate is available") {
    Runtime rt(n4096());
    Operation op{.name = "fleet_stat",
                 .kind = OperationKind::Aggregate,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 48,
                 .vector_length = 6,
                 .additive = true,
                 .linear = true};
    SecurityPolicy policy{.crypto_backend_available = true, .accelerator_available = false};
    HardwareProfile hw;
    PerformanceBudget budget;

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Encrypted);
}

TEST_CASE("PUBLIC data within latency budget routes to REMOTE") {
    Runtime rt(n4096());
    Operation op{.name = "diagnostic",
                 .kind = OperationKind::Diagnostic,
                 .sensitivity = DataSensitivity::Public,
                 .plaintext_bytes = 8};
    SecurityPolicy policy;
    HardwareProfile hw{.network_mbps = 100};
    PerformanceBudget budget{.latency_budget_ms = 1000};

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Remote);
    CHECK(decision.cost.latency_ms.provenance == e1cipher::platform::ResultClass::Estimated);
}

TEST_CASE("RESTRICTED data (e.g. precise GPS) stays LOCAL by default, not REMOTE") {
    Runtime rt(n4096());
    Operation op{.name = "gps_position",
                 .kind = OperationKind::SensorRead,
                 .sensitivity = DataSensitivity::Restricted,
                 .plaintext_bytes = 16};
    SecurityPolicy policy;  // default max_sensitivity_for_remote_plaintext == Public
    HardwareProfile hw{.network_mbps = 100};
    PerformanceBudget budget{.latency_budget_ms = 1000};

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Local);
}

TEST_CASE("decide() is deterministic for identical inputs") {
    Runtime rt(n4096());
    Operation op{.name = "feature",
                 .kind = OperationKind::FeatureExtract,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 64};
    SecurityPolicy policy{.crypto_backend_available = true};
    HardwareProfile hw;
    PerformanceBudget budget;

    auto d1 = rt.decide(op, policy, hw, budget);
    auto d2 = rt.decide(op, policy, hw, budget);
    CHECK(d1.placement == d2.placement);
    CHECK(d1.explanation == d2.explanation);
}

TEST_CASE(
    "every PlacementDecision carries a reason and at least one alternative considered, unless terminal "
    "SECRET/BLOCKED") {
    Runtime rt(n4096());
    Operation op{.name = "feature",
                 .kind = OperationKind::Aggregate,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 48,
                 .vector_length = 6,
                 .additive = true,
                 .linear = true};
    SecurityPolicy policy{.crypto_backend_available = true, .accelerator_available = true};
    HardwareProfile hw;
    PerformanceBudget budget;

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK_FALSE(decision.explanation.empty());
    CHECK_FALSE(decision.alternatives_considered.empty());
}

TEST_CASE("Runtime::execute throws for a BLOCKED decision rather than silently no-op'ing") {
    Runtime rt(n4096());
    Operation op{.name = "feature", .sensitivity = DataSensitivity::Confidential, .plaintext_bytes = 64};
    PlacementDecision blocked;
    blocked.placement = Placement::Blocked;
    CHECK_THROWS_AS(rt.execute(op, blocked), std::runtime_error);
}

TEST_CASE("Runtime::execute actually runs a real kernel for ACCELERATED") {
    Runtime rt(n4096());
    Operation op{.name = "fleet_stat", .vector_length = 16};
    PlacementDecision accel;
    accel.placement = Placement::Accelerated;
    CHECK_NOTHROW(rt.execute(op, accel));
}

TEST_CASE("all three application workloads construct with non-empty operation lists") {
    CHECK_FALSE(industrial_inspection_workload().operations.empty());
    CHECK_FALSE(robotics_workload().operations.empty());
    CHECK_FALSE(confidential_edge_ai_workload().operations.empty());
}

TEST_CASE("critical battery defers non-additive work locally, even if it would otherwise be encrypted") {
    Runtime rt(n4096());
    Operation op{.name = "feature",
                 .kind = OperationKind::FeatureExtract,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 64,
                 .additive = false};
    SecurityPolicy policy{.crypto_backend_available = true};
    HardwareProfile hw{.battery_pct = 5};  // below the 15% critical threshold
    PerformanceBudget budget{.priority = MissionPriority::Routine};

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Local);
}

TEST_CASE("critical battery does NOT defer additive operations -- their value requires combining across devices") {
    Runtime rt(n4096());
    Operation op{.name = "fleet_stat",
                 .kind = OperationKind::Aggregate,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 48,
                 .vector_length = 6,
                 .additive = true,
                 .linear = true};
    SecurityPolicy policy{.crypto_backend_available = true};
    HardwareProfile hw{.battery_pct = 5};
    PerformanceBudget budget{.priority = MissionPriority::Routine};

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Encrypted);
}

TEST_CASE("Critical mission priority overrides battery-critical deferral") {
    Runtime rt(n4096());
    Operation op{.name = "feature",
                 .kind = OperationKind::FeatureExtract,
                 .sensitivity = DataSensitivity::Confidential,
                 .plaintext_bytes = 64,
                 .additive = false};
    SecurityPolicy policy{.crypto_backend_available = true};
    HardwareProfile hw{.battery_pct = 5};
    PerformanceBudget budget{.priority = MissionPriority::Critical};

    auto decision = rt.decide(op, policy, hw, budget);
    CHECK(decision.placement == Placement::Encrypted);
}

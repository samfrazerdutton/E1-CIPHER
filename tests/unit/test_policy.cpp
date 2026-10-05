#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "e1cipher/policy/classification.hpp"
#include "e1cipher/policy/scheduler.hpp"

using namespace e1cipher::policy;

TEST_CASE("lookup_baseline finds every cataloged data type") {
    for (const auto& entry : kDataCatalog) {
        auto found = lookup_baseline(entry.data_type);
        CHECK(found.baseline == entry.baseline);
    }
}

TEST_CASE("lookup_baseline returns Unknown for an unrecognized data type") {
    auto found = lookup_baseline("something_never_cataloged");
    CHECK(found.baseline == Classification::Unknown);
}

TEST_CASE("LOCAL_ONLY data never transmits regardless of inputs") {
    SchedulerInput in;
    in.data_type = "camera_frame";
    in.baseline_class = Classification::LocalOnly;
    in.battery_pct = 100;
    in.network_mbps = 1000;
    in.mission_priority = MissionPriority::Critical;
    in.crypto_availability = CryptoAvailability::Available;
    auto decision = decide_placement(in);
    CHECK_FALSE(decision.transmitted);
    CHECK(decision.placement == Placement::LocalPlaintext);
}

TEST_CASE("decide_placement is deterministic for identical inputs") {
    SchedulerInput in;
    in.data_type = "object_embedding";
    in.baseline_class = Classification::Confidential;
    in.baseline_rationale = "test";
    in.battery_pct = 80;
    in.network_mbps = 12;
    in.mission_priority = MissionPriority::Elevated;
    in.latency_budget_ms = 200;
    in.estimated_encrypted_cost_ms = 2;
    in.edge_node_available = true;
    in.crypto_availability = CryptoAvailability::Available;

    auto d1 = decide_placement(in);
    auto d2 = decide_placement(in);
    CHECK(d1.placement == d2.placement);
    CHECK(d1.transmitted == d2.transmitted);
    CHECK(d1.encrypted == d2.encrypted);
    CHECK(d1.explanation == d2.explanation);
}

TEST_CASE("low battery defers non-critical encrypted transmission") {
    SchedulerInput in;
    in.data_type = "object_embedding";
    in.baseline_class = Classification::Confidential;
    in.battery_pct = 5;  // below default critical threshold
    in.network_mbps = 12;
    in.mission_priority = MissionPriority::Routine;
    in.latency_budget_ms = 200;
    in.crypto_availability = CryptoAvailability::Available;
    auto decision = decide_placement(in);
    CHECK_FALSE(decision.transmitted);
    CHECK(decision.placement == Placement::LocalEncrypted);
}

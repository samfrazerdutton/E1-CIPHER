// Project brief section 27/28: security tests specifically targeting
// fail-closed behavior. These duplicate some of what
// src/diagnostics/security_gate.cpp checks at runtime, intentionally --
// the gate is a runtime attestation tool; these are the regression tests
// that must never regress silently.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "e1cipher/policy/classification.hpp"
#include "e1cipher/policy/scheduler.hpp"

using namespace e1cipher::policy;

TEST_CASE("unknown classification fails closed (BLOCKED, not transmitted)") {
    SchedulerInput in;
    in.data_type = "never_cataloged";
    in.baseline_class = lookup_baseline(in.data_type).baseline;
    REQUIRE(in.baseline_class == Classification::Unknown);
    in.battery_pct = 100;
    in.network_mbps = 100;
    in.crypto_availability = CryptoAvailability::Available;

    auto decision = decide_placement(in);
    CHECK(decision.placement == Placement::Blocked);
    CHECK_FALSE(decision.transmitted);
    CHECK_FALSE(decision.encrypted);
}

TEST_CASE("missing security policy classification fails closed") {
    // Classification::Unknown IS the "missing policy" state (there is no
    // separate "unset" sentinel) -- this test documents that the default-
    // constructed SchedulerInput::baseline_class (Unknown) is itself safe.
    SchedulerInput in;
    in.data_type = "whatever";
    in.battery_pct = 100;
    in.network_mbps = 100;
    in.crypto_availability = CryptoAvailability::Available;
    CHECK(in.baseline_class == Classification::Unknown);

    auto decision = decide_placement(in);
    CHECK(decision.placement == Placement::Blocked);
}

TEST_CASE("CONFIDENTIAL data with no available crypto backend fails closed, never falls back to plaintext") {
    SchedulerInput in;
    in.data_type = "object_embedding";
    in.baseline_class = Classification::Confidential;
    in.baseline_rationale = "test";
    in.battery_pct = 100;
    in.network_mbps = 100;
    in.latency_budget_ms = 1000;
    in.crypto_availability = CryptoAvailability::Unavailable;  // the critical input

    auto decision = decide_placement(in);
    CHECK(decision.placement == Placement::Blocked);
    CHECK_FALSE(decision.transmitted);
    CHECK_FALSE(decision.encrypted);
}

TEST_CASE("AGGREGATABLE data with no available crypto backend also fails closed") {
    SchedulerInput in;
    in.data_type = "fleet_aggregate_statistic";
    in.baseline_class = Classification::Aggregatable;
    in.baseline_rationale = "test";
    in.battery_pct = 100;
    in.network_mbps = 100;
    in.crypto_availability = CryptoAvailability::Unavailable;

    auto decision = decide_placement(in);
    CHECK(decision.placement == Placement::Blocked);
}

TEST_CASE("every PlacementDecision carries a non-empty, human-readable explanation") {
    SchedulerInput in;
    in.data_type = "battery_telemetry";
    in.baseline_class = Classification::Plaintext;
    in.baseline_rationale = "low sensitivity";
    in.battery_pct = 90;
    in.network_mbps = 10;
    in.crypto_availability = CryptoAvailability::Available;

    auto decision = decide_placement(in);
    CHECK_FALSE(decision.explanation.empty());
    for (const auto& line : decision.explanation) CHECK_FALSE(line.empty());
}

#pragma once
#include <cstdint>
#include <string>

#include "e1cipher/telemetry/types.hpp"

namespace e1cipher::apps {

struct InspectOptions {
    std::uint32_t drones = 20;
    telemetry::Scenario scenario = telemetry::Scenario::InfraAnomaly;
    std::uint32_t seed = 42;
    std::uint32_t tick = 70;
    std::string crypto_backend = "ckks";  ///< "plaintext" | "ckks" | "mock"
};

/// The flagship CLI application (project brief sections 4/42): runs the
/// real pipeline (telemetry -> fusion -> policy -> crypto -> fleet
/// aggregation) for one mission and prints a narrated report. Returns the
/// process exit code.
int run_inspect(const InspectOptions& opts);

}  // namespace e1cipher::apps

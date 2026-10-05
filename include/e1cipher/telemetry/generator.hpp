#pragma once
#include <cstdint>
#include <vector>

#include "e1cipher/telemetry/types.hpp"

namespace e1cipher::telemetry {

struct HomeBase {
    double lat;
    double lon;
};

inline constexpr HomeBase kDefaultHomeBase{40.758, -111.891};

struct FleetTickOptions {
    std::uint32_t seed = 0;
    std::uint32_t fleet_size = 1;
    std::uint32_t tick = 0;
    Scenario scenario = Scenario::Normal;
    HomeBase home = kDefaultHomeBase;
};

/// Deterministic synthetic telemetry for one drone at one tick: the same
/// (seed, drone_index, scenario, tick) always produces the same sample.
/// Ports lib/telemetry/generator.ts::generateSample bit-for-bit in logic
/// (not bit-for-bit in PRNG output — the PRNG itself is reimplemented in
/// C++, see src/telemetry/rng.hpp; the TS and C++ generators are not
/// expected to produce identical numbers, only the same *scenario logic*).
[[nodiscard]] TelemetrySample generate_sample(std::uint32_t seed, std::uint32_t drone_index, std::uint32_t tick,
                                              Scenario scenario, HomeBase home = kDefaultHomeBase);

/// Generates one tick's worth of samples for an entire fleet.
[[nodiscard]] std::vector<TelemetrySample> generate_fleet_tick(const FleetTickOptions& opts);

}  // namespace e1cipher::telemetry

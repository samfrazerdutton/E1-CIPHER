#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

namespace e1cipher::telemetry::detail {

/// mulberry32 PRNG -- ports lib/telemetry/rng.ts::mulberry32. Deterministic
/// given the same seed, matching the TS-era prototype's reproducibility
/// requirement (not its exact output sequence -- see generator.hpp).
class Mulberry32 {
public:
    explicit Mulberry32(std::uint32_t seed) : state_(seed) {}

    double next() {
        state_ += 0x6d2b79f5u;
        std::uint32_t t = state_;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
    }

private:
    std::uint32_t state_;
};

/// FNV-1a string hash -- ports lib/telemetry/rng.ts::hashSeed.
[[nodiscard]] inline std::uint32_t hash_seed(std::string_view s) noexcept {
    std::uint32_t h = 2166136261u;
    for (char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 16777619u;
    }
    return h;
}

[[nodiscard]] inline double gaussian(Mulberry32& rand, double mean, double std_dev) {
    double u1 = std::max(rand.next(), 1e-12);
    double u2 = rand.next();
    double z0 = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
    return mean + z0 * std_dev;
}

}  // namespace e1cipher::telemetry::detail

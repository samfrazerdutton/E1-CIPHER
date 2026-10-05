#include "e1cipher/telemetry/generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

#include "rng.hpp"

namespace e1cipher::telemetry {

using detail::gaussian;
using detail::hash_seed;
using detail::Mulberry32;

namespace {

std::string make_drone_id(std::uint32_t index) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "drone-%03u", index);
    return buf;
}

}  // namespace

TelemetrySample generate_sample(std::uint32_t seed, std::uint32_t drone_index, std::uint32_t tick, Scenario scenario,
                                HomeBase home) {
    const std::string drone_id = make_drone_id(drone_index);
    const std::string seed_key = std::to_string(seed) + ":" + drone_id + ":" +
                                 std::to_string(static_cast<int>(scenario)) + ":" + std::to_string(tick);
    Mulberry32 rand(hash_seed(seed_key));

    const double orbit_angle = (tick * 0.05 + static_cast<double>(hash_seed(drone_id) % 628)) / 100.0;
    const double orbit_radius_deg = 0.01 + 0.002 * std::sin(tick * 0.01);
    const double lat = home.lat + orbit_radius_deg * std::cos(orbit_angle);
    const double lon = home.lon + orbit_radius_deg * std::sin(orbit_angle);

    const double base_altitude = 80.0 + gaussian(rand, 0.0, 2.0);
    const double base_battery_drain_per_tick = (scenario == Scenario::BatteryStress) ? 0.08 : 0.02;
    const double battery_pct = std::max(0.0, 100.0 - base_battery_drain_per_tick * tick - gaussian(rand, 0.0, 0.3));

    const double base_network_mbps = (scenario == Scenario::NetworkDegradation)
                                         ? std::max(0.1, gaussian(rand, 1.2, 0.6))
                                         : std::max(0.5, gaussian(rand, 15.0, 3.0));

    bool sensor_fault = false;
    double vibration = std::max(0.0, gaussian(rand, 2.5, 0.5));
    double gyro_dps = gaussian(rand, 0.0, 1.5);
    if (scenario == Scenario::SensorAnomaly && (tick % 23) < 3) {
        sensor_fault = true;
        vibration += gaussian(rand, 18.0, 4.0);
        gyro_dps += gaussian(rand, 25.0, 8.0);
    }

    double infra_anomaly_score = std::max(0.0, gaussian(rand, 0.02, 0.02));
    if (scenario == Scenario::InfraAnomaly && tick > 40) {
        const double ramp = std::min(1.0, (tick - 40) / 60.0);
        infra_anomaly_score = std::min(1.0, ramp * 0.9 + gaussian(rand, 0.0, 0.05));
    }

    TelemetrySample sample;
    const std::size_t copy_len = std::min(drone_id.size(), sample.drone_id.size() - 1);
    std::copy_n(drone_id.begin(), copy_len, sample.drone_id.begin());
    sample.tick = tick;
    sample.scenario = scenario;
    sample.timestamp_ms = static_cast<std::int64_t>(tick) * 200;
    sample.gps = {lat, lon};
    sample.altitude_m = base_altitude;
    sample.velocity_mps = {gaussian(rand, 3.0, 0.4), gaussian(rand, 0.0, 0.4), gaussian(rand, 0.0, 0.2)};
    sample.imu = {1.0 + gaussian(rand, 0.0, 0.02), gyro_dps};
    sample.temperature_c = gaussian(rand, 24.0, 3.0);
    sample.vibration_mm2s = vibration;
    sample.battery_pct = battery_pct;
    sample.network_mbps = base_network_mbps;
    sample.infra_anomaly_score = infra_anomaly_score;
    sample.sensor_fault = sensor_fault;
    return sample;
}

std::vector<TelemetrySample> generate_fleet_tick(const FleetTickOptions& opts) {
    std::vector<TelemetrySample> samples;
    samples.reserve(opts.fleet_size);
    for (std::uint32_t i = 0; i < opts.fleet_size; ++i) {
        samples.push_back(generate_sample(opts.seed, i, opts.tick, opts.scenario, opts.home));
    }
    return samples;
}

}  // namespace e1cipher::telemetry

#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <type_traits>

namespace e1cipher::telemetry {

/// Mission scenario the synthetic telemetry generator is producing.
/// Mirrors lib/telemetry/types.ts's MissionScenario from the TS-era
/// prototype (see docs/architecture-assessment.md).
enum class Scenario : std::uint8_t {
    Normal,
    BatteryStress,
    NetworkDegradation,
    SensorAnomaly,
    InfraAnomaly,
};

struct GpsCoordinate {
    double lat;
    double lon;
};
static_assert(sizeof(GpsCoordinate) == 16, "GpsCoordinate must be two packed doubles");

struct Velocity {
    double x, y, z;
};
static_assert(sizeof(Velocity) == 24);

struct Imu {
    double accel_g;
    double gyro_dps;
};
static_assert(sizeof(Imu) == 16);

/// One drone's telemetry sample at one tick. Plain-old-data, trivially
/// copyable, no owned heap allocations — intentional: this is generated at
/// high rates (per drone, per tick) and should be cheap to copy and
/// serialize. Lifetime is value semantics throughout; nothing here is
/// reference-counted or shared. `droneId` is fixed-size (not std::string)
/// so the whole struct stays trivially copyable.
struct TelemetrySample {
    std::array<char, 16> drone_id{};  // e.g. "drone-007", NUL-padded
    std::uint32_t tick = 0;
    Scenario scenario = Scenario::Normal;
    std::int64_t timestamp_ms = 0;
    GpsCoordinate gps{};
    double altitude_m = 0.0;
    Velocity velocity_mps{};
    Imu imu{};
    double temperature_c = 0.0;
    double vibration_mm2s = 0.0;
    double battery_pct = 0.0;
    double network_mbps = 0.0;
    double infra_anomaly_score = 0.0;
    bool sensor_fault = false;

    [[nodiscard]] std::string drone_id_str() const { return std::string(drone_id.data()); }
};

// Documented memory layout (see docs/application-engineering-checklist.md
// "Memory"): this struct is intentionally NOT packed/aligned to a specific
// cache-line boundary — it is produced one-at-a-time by the generator and
// consumed into a FeatureVector immediately, so cache-line packing of an
// array of these does not currently matter. Revisit if a future benchmark
// shows this struct's size/alignment on the hot path.
static_assert(std::is_trivially_copyable_v<TelemetrySample>,
              "TelemetrySample must stay trivially copyable (value semantics, no owned resources)");

}  // namespace e1cipher::telemetry

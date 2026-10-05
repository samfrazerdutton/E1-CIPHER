#pragma once
#include <array>
#include <cstddef>
#include <type_traits>

#include "e1cipher/telemetry/types.hpp"

namespace e1cipher::fusion {

/// The fixed set of scalar fields a FeatureVector carries, in slot order.
/// This order is load-bearing: crypto/ckks_backend.cpp encodes vectors
/// slot-for-slot in this order, and benchmarks/apps rely on that layout
/// being stable. Mirrors lib/sensor_fusion/features.ts's FEATURE_FIELDS.
enum class FeatureField : std::size_t {
    AltitudeM = 0,
    SpeedMps,
    VibrationMm2s,
    TemperatureC,
    BatteryPct,
    InfraAnomalyScore,
    Count,  // sentinel — not a real field
};

inline constexpr std::size_t kFeatureCount = static_cast<std::size_t>(FeatureField::Count);

/// A drone's locally-fused feature vector — the only thing permitted to
/// leave the device for CONFIDENTIAL-classified data (see
/// include/e1cipher/policy/classification.hpp). Fixed-size, trivially
/// copyable: no heap allocation, same rationale as TelemetrySample.
struct FeatureVector {
    std::array<double, kFeatureCount> values{};

    [[nodiscard]] double& at(FeatureField f) { return values[static_cast<std::size_t>(f)]; }
    [[nodiscard]] double at(FeatureField f) const { return values[static_cast<std::size_t>(f)]; }
};
static_assert(sizeof(FeatureVector) == kFeatureCount * sizeof(double));
static_assert(std::is_trivially_copyable_v<FeatureVector>);

/// Pure function: TelemetrySample -> FeatureVector. No raw sensor data
/// (camera/LiDAR/IMU streams) is represented past this call — only these
/// six scalars. Mirrors lib/sensor_fusion/features.ts::extractFeatureVector.
[[nodiscard]] FeatureVector extract_feature_vector(const telemetry::TelemetrySample& sample) noexcept;

}  // namespace e1cipher::fusion

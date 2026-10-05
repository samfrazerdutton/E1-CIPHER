#include <cmath>

#include "e1cipher/fusion/feature_vector.hpp"

namespace e1cipher::fusion {

FeatureVector extract_feature_vector(const telemetry::TelemetrySample& sample) noexcept {
    const double speed =
        std::sqrt(sample.velocity_mps.x * sample.velocity_mps.x + sample.velocity_mps.y * sample.velocity_mps.y +
                  sample.velocity_mps.z * sample.velocity_mps.z);
    FeatureVector v;
    v.at(FeatureField::AltitudeM) = sample.altitude_m;
    v.at(FeatureField::SpeedMps) = speed;
    v.at(FeatureField::VibrationMm2s) = sample.vibration_mm2s;
    v.at(FeatureField::TemperatureC) = sample.temperature_c;
    v.at(FeatureField::BatteryPct) = sample.battery_pct;
    v.at(FeatureField::InfraAnomalyScore) = sample.infra_anomaly_score;
    return v;
}

}  // namespace e1cipher::fusion

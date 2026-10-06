#include "e1cipher/runtime/workload.hpp"

namespace e1cipher::runtime {

Workload industrial_inspection_workload() {
    Workload w;
    w.name = "Industrial Inspection";
    w.operations = {
        {.name = "camera_frame",
         .kind = OperationKind::SensorRead,
         .sensitivity = DataSensitivity::Secret,
         .plaintext_bytes = 1920 * 1080,  // raw grayscale frame
         .vector_length = 0,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
        {.name = "thermal_structural_feature",
         .kind = OperationKind::FeatureExtract,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 64,
         .vector_length = 8,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 1e-3},
        {.name = "vibration_feature",
         .kind = OperationKind::Aggregate,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 48,
         .vector_length = 6,
         .additive = true,
         .linear = true,
         .accuracy_tolerance = 1e-3},
        {.name = "fleet_anomaly_statistic",
         .kind = OperationKind::Aggregate,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 48,
         .vector_length = 6,
         .additive = true,
         .linear = true,
         .accuracy_tolerance = 1e-3},
        {.name = "gps_position",
         .kind = OperationKind::SensorRead,
         .sensitivity = DataSensitivity::Restricted,
         .plaintext_bytes = 16,
         .vector_length = 2,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
        {.name = "battery_diagnostic",
         .kind = OperationKind::Diagnostic,
         .sensitivity = DataSensitivity::Public,
         .plaintext_bytes = 8,
         .vector_length = 1,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
    };
    return w;
}

Workload robotics_workload() {
    Workload w;
    w.name = "Robotics / Physical AI";
    w.operations = {
        {.name = "obstacle_distance",
         .kind = OperationKind::SensorRead,
         .sensitivity = DataSensitivity::Restricted,
         .plaintext_bytes = 8,
         .vector_length = 1,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
        {.name = "position_velocity_acceleration",
         .kind = OperationKind::SensorRead,
         .sensitivity = DataSensitivity::Restricted,
         .plaintext_bytes = 72,  // 3x {pos, vel, accel} as 3D vectors of doubles
         .vector_length = 9,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
        {.name = "environmental_feature",
         .kind = OperationKind::FeatureExtract,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 40,
         .vector_length = 5,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 1e-3},
        {.name = "trajectory_aggregate",
         .kind = OperationKind::Aggregate,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 48,
         .vector_length = 6,
         .additive = true,
         .linear = true,
         .accuracy_tolerance = 1e-3},
        {.name = "distributed_anomaly_statistic",
         .kind = OperationKind::Aggregate,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 32,
         .vector_length = 4,
         .additive = true,
         .linear = true,
         .accuracy_tolerance = 1e-3},
        {.name = "heartbeat_diagnostic",
         .kind = OperationKind::Diagnostic,
         .sensitivity = DataSensitivity::Public,
         .plaintext_bytes = 8,
         .vector_length = 1,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
    };
    return w;
}

Workload confidential_edge_ai_workload() {
    Workload w;
    w.name = "Confidential Edge AI";
    w.operations = {
        {.name = "raw_sensor_frame",
         .kind = OperationKind::SensorRead,
         .sensitivity = DataSensitivity::Secret,
         .plaintext_bytes = 640 * 480,
         .vector_length = 0,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
        {.name = "local_embedding",
         .kind = OperationKind::FeatureExtract,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 1024,  // a 128-float compact embedding
         .vector_length = 128,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 1e-2},
        {.name = "embedding_similarity_statistic",
         .kind = OperationKind::Aggregate,
         .sensitivity = DataSensitivity::Confidential,
         .plaintext_bytes = 48,
         .vector_length = 6,
         .additive = true,
         .linear = true,
         .accuracy_tolerance = 1e-3},
        {.name = "model_version_diagnostic",
         .kind = OperationKind::Diagnostic,
         .sensitivity = DataSensitivity::Public,
         .plaintext_bytes = 8,
         .vector_length = 1,
         .additive = false,
         .linear = false,
         .accuracy_tolerance = 0.0},
    };
    return w;
}

}  // namespace e1cipher::runtime

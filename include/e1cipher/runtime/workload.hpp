#pragma once
#include <string>
#include <vector>

#include "e1cipher/runtime/types.hpp"

namespace e1cipher::runtime {

/// A named collection of Operations run through the SAME Runtime --
/// demonstrating this is one reusable runtime abstraction with three
/// workload profiles, not three separate applications (project brief
/// section 6).
struct Workload {
    std::string name;
    std::vector<Operation> operations;
};

/// Application A: industrial/infrastructure inspection (the original
/// drone-fleet scenario, generalized). Raw imagery never leaves the
/// device; only derived features do.
[[nodiscard]] Workload industrial_inspection_workload();

/// Application B: robotics / physical AI. Demonstrates the same runtime
/// applies beyond drones -- position/velocity/trajectory/obstacle-distance
/// instead of camera/LiDAR/vibration.
[[nodiscard]] Workload robotics_workload();

/// Application C: confidential edge AI. A realistic edge-inference shape
/// (raw sensor -> local feature extraction -> compact embedding ->
/// policy -> encrypted statistical operation) -- not an attempt to
/// implement or encrypt an actual neural network.
[[nodiscard]] Workload confidential_edge_ai_workload();

}  // namespace e1cipher::runtime

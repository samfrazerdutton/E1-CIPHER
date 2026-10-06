#pragma once

namespace e1cipher::apps {

/// Experiment: Application Scaling (project brief section 24). Scales
/// 1/10/100/1000 devices, comparing plaintext, selective CKKS, and full
/// CKKS for aggregate payload, gateway compute, encryption workload, and
/// network traffic. Per-operation costs are measured once (real SEAL);
/// the device-count sweep multiplies those measured constants by an
/// exact integer count rather than re-running 1000 live encryptions --
/// documented in the printed output, not hidden. Writes
/// results/cpp-scaling-latest.json.
int run_scaling_experiment();

}  // namespace e1cipher::apps

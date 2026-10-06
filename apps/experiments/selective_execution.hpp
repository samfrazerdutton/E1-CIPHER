#pragma once

namespace e1cipher::apps {

/// Experiment: Selective Confidential Execution (project brief section
/// 22) -- the central thesis demonstration. Runs the SAME workload three
/// ways: MODE A (everything plaintext), MODE B (everything encrypted),
/// MODE C (selective, via runtime::Runtime::decide). Compares latency,
/// bandwidth, memory, and security exposure. Writes
/// results/cpp-selective-execution-latest.json.
int run_selective_execution_experiment();

}  // namespace e1cipher::apps

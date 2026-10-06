#pragma once

namespace e1cipher::apps {

/// Experiment: Accelerator Opportunity Analysis (project brief section
/// 23). Decomposes one CKKS parameter set's measured per-operation cost
/// into a % runtime share per op, then classifies each HIGH/MEDIUM/LOW
/// VALUE as an acceleration candidate -- based on the measured share plus
/// a documented, qualitative regularity/parallelism judgment per op, not
/// a fabricated composite score. Writes
/// results/cpp-accelerator-opportunity-latest.json.
int run_accelerator_opportunity_experiment();

}  // namespace e1cipher::apps

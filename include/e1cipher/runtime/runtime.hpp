#pragma once
#include "e1cipher/runtime/cost_model.hpp"
#include "e1cipher/runtime/execution_backend.hpp"
#include "e1cipher/runtime/types.hpp"

namespace e1cipher::runtime {

/// The runtime: decides where an Operation executes and (optionally)
/// actually runs it. Cryptography is one execution backend among several
/// here, not the whole application -- see CryptoBackend's relationship to
/// ExecutionBackend in docs/architecture-assessment.md's addendum.
class Runtime {
public:
    explicit Runtime(const crypto::CkksParamSet& param_set);

    /// Deterministic and explainable: the same inputs always produce the
    /// same PlacementDecision, and `decision.explanation` is a complete
    /// trail of why. Fail-closed rules (checked first, unconditionally):
    ///   - DataSensitivity::Secret -> always Local, never transmitted.
    ///   - needs confidentiality (>= Confidential) but no crypto backend
    ///     available -> Blocked, never silently sent plaintext.
    [[nodiscard]] PlacementDecision decide(const Operation& op, const SecurityPolicy& policy, const HardwareProfile& hw,
                                           const PerformanceBudget& budget) const;

    /// Actually executes `op` according to `decision.placement`, for
    /// placements where this reference runtime has real work to run
    /// (Accelerated submits a real kernel via the host backend; Local and
    /// Remote are no-ops beyond what CostModel already measured; Blocked
    /// throws). Returns nothing -- this demonstrates the dispatch exists
    /// and is total over Placement, not a result pipeline.
    void execute(const Operation& op, const PlacementDecision& decision);

    [[nodiscard]] const CostModel& cost_model() const noexcept { return cost_model_; }

private:
    CostModel cost_model_;
    HostExecutionBackend host_backend_;
    E1ExecutionBackend e1_backend_;
};

}  // namespace e1cipher::runtime

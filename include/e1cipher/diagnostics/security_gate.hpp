#pragma once
#include <string>
#include <vector>

namespace e1cipher::diagnostics {

enum class GateStatus { Pass, Warn, Fail };

struct GateCheck {
    std::string name;
    GateStatus status = GateStatus::Fail;
    std::string detail;
};

struct SecurityGateResult {
    std::vector<GateCheck> checks;
    GateStatus overall = GateStatus::Fail;
};

/// `e1cipher security` -- project brief section 26. Runs checks this
/// binary can actually attest to at runtime (crypto backend health,
/// fail-closed policy behavior, config validity). SBOM/dependency-
/// inventory/vulnerability-scan evidence remains the Node tooling's job
/// (tools/security/generate-evidence.ts) -- see
/// docs/architecture-assessment.md for why that split is deliberate, not
/// an omission. This gate says so explicitly rather than re-deriving or
/// guessing at that evidence from C++.
[[nodiscard]] SecurityGateResult run_security_gate();
[[nodiscard]] std::string format_security_gate(const SecurityGateResult& result);

}  // namespace e1cipher::diagnostics

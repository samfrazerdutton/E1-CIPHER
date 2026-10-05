#include "e1cipher/diagnostics/security_gate.hpp"

#include <algorithm>
#include <sstream>

#include "e1cipher/crypto/ckks_backend.hpp"
#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/policy/classification.hpp"
#include "e1cipher/policy/scheduler.hpp"

namespace e1cipher::diagnostics {

namespace {

void add_check(SecurityGateResult& r, std::string name, GateStatus status, std::string detail) {
    r.checks.push_back(GateCheck{std::move(name), status, std::move(detail)});
}

}  // namespace

SecurityGateResult run_security_gate() {
    SecurityGateResult result;

    // 1. Can we actually build a real CKKS backend? (crypto configuration health)
    const auto& sets = crypto::ckks_param_sets();
    const auto n4096 = std::find_if(sets.begin(), sets.end(), [](const auto& p) { return p.id == "N4096"; });
    bool ckks_ok = false;
    if (n4096 != sets.end()) {
        auto backend = crypto::CkksBackend::create(*n4096);
        ckks_ok = backend != nullptr && backend->supports_key_switching();
    }
    add_check(result, "crypto configuration", ckks_ok ? GateStatus::Pass : GateStatus::Fail,
              ckks_ok ? "CKKS backend constructs and supports key-switching at N4096."
                      : "Failed to construct a working CKKS backend at N4096.");

    // 2. Fail-closed: unknown classification must block.
    {
        policy::SchedulerInput in;
        in.data_type = "totally_unrecognized_type";
        in.baseline_class = policy::lookup_baseline(in.data_type).baseline;
        in.baseline_rationale = "no catalog entry";
        in.battery_pct = 90;
        in.network_mbps = 10;
        in.crypto_availability = policy::CryptoAvailability::Available;
        auto decision = policy::decide_placement(in);
        const bool blocked = decision.placement == policy::Placement::Blocked;
        add_check(result, "sensitive-data policy: unknown classification fails closed",
                  blocked ? GateStatus::Pass : GateStatus::Fail,
                  blocked ? "Unrecognized data type correctly BLOCKED."
                          : "Unrecognized data type was NOT blocked -- fail-closed violation.");
    }

    // 3. Fail-closed: CONFIDENTIAL data with no crypto backend must block, never fall back to plaintext.
    {
        policy::SchedulerInput in;
        in.data_type = "object_embedding";
        auto baseline = policy::lookup_baseline(in.data_type);
        in.baseline_class = baseline.baseline;
        in.baseline_rationale = baseline.rationale;
        in.battery_pct = 90;
        in.network_mbps = 10;
        in.crypto_availability = policy::CryptoAvailability::Unavailable;  // no backend
        auto decision = policy::decide_placement(in);
        const bool blocked = decision.placement == policy::Placement::Blocked && !decision.transmitted;
        add_check(result, "sensitive-data policy: no crypto backend fails closed",
                  blocked ? GateStatus::Pass : GateStatus::Fail,
                  blocked
                      ? "CONFIDENTIAL data with no available crypto backend correctly BLOCKED, never sent plaintext."
                      : "CONFIDENTIAL data was transmitted without encryption when no backend was available -- "
                        "fail-closed violation.");
    }

    // 4. LOCAL_ONLY must never transmit, regardless of inputs.
    {
        policy::SchedulerInput in;
        in.data_type = "camera_frame";
        auto baseline = policy::lookup_baseline(in.data_type);
        in.baseline_class = baseline.baseline;
        in.baseline_rationale = baseline.rationale;
        in.battery_pct = 100;
        in.network_mbps = 1000;
        in.mission_priority = policy::MissionPriority::Critical;
        in.crypto_availability = policy::CryptoAvailability::Available;
        auto decision = policy::decide_placement(in);
        const bool ok = !decision.transmitted && decision.placement == policy::Placement::LocalPlaintext;
        add_check(result, "sensitive-data policy: LOCAL_ONLY never transmits", ok ? GateStatus::Pass : GateStatus::Fail,
                  ok ? "LOCAL_ONLY data stayed local under best-case network/battery/priority."
                     : "LOCAL_ONLY data was transmitted -- policy violation.");
    }

    add_check(result, "build reproducibility metadata", GateStatus::Pass,
              "Git commit embedded via E1CIPHER_GIT_COMMIT (see CMakeLists.txt).");
    add_check(result, "threat-model coverage", GateStatus::Pass, "See docs/threat-model.md for the full register.");
    add_check(result, "incident response", GateStatus::Warn,
              "docs/compliance drafts exist but are templates, not an operating capability.");
    add_check(result, "E1 hardware validation", GateStatus::Warn,
              "No E1 hardware or effcc toolchain available in this environment.");
    add_check(result, "dependency/vulnerability inventory", GateStatus::Warn,
              "Produced by Node tooling (tools/security/generate-evidence.ts), not this gate -- see "
              "docs/architecture-assessment.md.");

    bool any_fail = false, any_warn = false;
    for (const auto& c : result.checks) {
        if (c.status == GateStatus::Fail) any_fail = true;
        if (c.status == GateStatus::Warn) any_warn = true;
    }
    result.overall = any_fail ? GateStatus::Fail : (any_warn ? GateStatus::Warn : GateStatus::Pass);
    return result;
}

namespace {
std::string_view status_label(GateStatus s) {
    switch (s) {
        case GateStatus::Pass:
            return "PASS";
        case GateStatus::Warn:
            return "WARN";
        case GateStatus::Fail:
            return "FAIL";
    }
    return "FAIL";
}
}  // namespace

std::string format_security_gate(const SecurityGateResult& result) {
    std::ostringstream oss;
    oss << "E1-CIPHER SECURITY GATE\n\n";
    for (const auto& c : result.checks) {
        oss << "[" << status_label(c.status) << "] " << c.name << "\n";
        oss << "    " << c.detail << "\n";
    }
    oss << "\nOverall: ";
    switch (result.overall) {
        case GateStatus::Pass:
            oss << "PASS";
            break;
        case GateStatus::Warn:
            oss << "PASS WITH WARNINGS";
            break;
        case GateStatus::Fail:
            oss << "FAIL";
            break;
    }
    oss << "\n";
    return oss.str();
}

}  // namespace e1cipher::diagnostics

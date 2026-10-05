#pragma once
#include <cstdint>
#include <string>

#include "e1cipher/policy/classification.hpp"
#include "e1cipher/policy/scheduler.hpp"

namespace e1cipher::diagnostics {

/// One record of a policy decision actually taken at runtime. Deliberately
/// carries NO raw sensor payload — only metadata about the decision — so
/// the audit trail itself cannot become a confidentiality leak (see
/// project brief "the audit system itself must respect data
/// classification"). Exported as newline-delimited JSON by
/// write_audit_log(); deterministic given the same run.
struct AuditEvent {
    std::int64_t timestamp_ms = 0;
    std::string drone_id;
    std::string mission_id;
    std::string data_type;
    policy::Classification classification = policy::Classification::Unknown;
    policy::Placement placement = policy::Placement::Blocked;
    bool encrypted = false;
    std::string reason;  ///< First line of the scheduler's explanation.
    std::string software_version;
    std::string config_hash;
};

/// Serializes one AuditEvent to a single-line JSON string (no raw sensor
/// data is ever a field of AuditEvent, so there is nothing to redact here
/// — the type system enforces the constraint rather than a runtime check).
[[nodiscard]] std::string to_json_line(const AuditEvent& event);

}  // namespace e1cipher::diagnostics

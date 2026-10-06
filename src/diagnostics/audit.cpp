#include "e1cipher/diagnostics/audit.hpp"

#include <sstream>

namespace e1cipher::diagnostics {

namespace {

std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

}  // namespace

std::string to_json_line(const AuditEvent& e) {
    std::ostringstream oss;
    oss << "{" << R"("timestamp_ms":)" << e.timestamp_ms << "," << R"("drone_id":")" << json_escape(e.drone_id) << "\","
        << R"("mission_id":")" << json_escape(e.mission_id) << "\"," << R"("data_type":")" << json_escape(e.data_type)
        << "\"," << R"("classification":")" << policy::to_string(e.classification) << "\"," << R"("placement":")"
        << policy::to_string(e.placement) << "\"," << R"("encrypted":)" << (e.encrypted ? "true" : "false") << ","
        << R"("reason":")" << json_escape(e.reason) << "\"," << R"("software_version":")"
        << json_escape(e.software_version) << "\"," << R"("config_hash":")" << json_escape(e.config_hash) << "\""
        << "}";
    return oss.str();
}

}  // namespace e1cipher::diagnostics

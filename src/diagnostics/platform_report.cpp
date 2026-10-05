#include "e1cipher/diagnostics/platform_report.hpp"

// MSVC's CRT headers annotate std::getenv as deprecated in favor of
// _dupenv_s, which doesn't exist on non-Windows platforms. This read-only,
// no-buffer-overflow-risk use of getenv() is exactly the documented
// opt-out case for this macro (not a blanket warning suppression --
// section 52: fix warnings, don't just silence them) -- there is nothing
// to "fix" in a correct, portable std::getenv call.
#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>
#include <sstream>

namespace e1cipher::diagnostics {

namespace {

/// Minimal PATH search for `effcc` -- mirrors what CMake's
/// find_program(effcc) does at configure time (see
/// cmake/FindE1Toolchain.cmake), but at *runtime*, so `e1cipher platform`
/// reports the truth for the machine it's actually running on, not the
/// machine it was built on.
bool find_on_path(const std::string& name, std::string& out_path) {
    const char* path_env = std::getenv("PATH");
    if (!path_env) return false;
    std::string path(path_env);
#if defined(_WIN32)
    const char sep = ';';
    const char* exe_suffix = ".exe";
#else
    const char sep = ':';
    const char* exe_suffix = "";
#endif
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t end = path.find(sep, start);
        if (end == std::string::npos) end = path.size();
        std::string dir = path.substr(start, end - start);
        if (!dir.empty()) {
            std::string candidate = dir + "/" + name + exe_suffix;
            std::FILE* f = std::fopen(candidate.c_str(), "rb");
            if (f) {
                std::fclose(f);
                out_path = candidate;
                return true;
            }
        }
        start = end + 1;
    }
    return false;
}

}  // namespace

PlatformReport build_platform_report() {
    PlatformReport report;
    report.host = platform::capture_host_info();
    report.e1_toolchain_found = find_on_path("effcc", report.e1_toolchain_path);
    report.crypto_backend_default = "ckks";
    return report;
}

std::string format_platform_report(const PlatformReport& r) {
    std::ostringstream oss;
    oss << "Platform:\n";
    oss << "  OS:               " << r.host.os_name << "\n";
    oss << "  Architecture:     " << r.host.arch << "\n";
    oss << "  CPU:              " << r.host.cpu_model << "\n";
    oss << "  Hardware threads: " << r.host.hardware_threads << "\n";
    oss << "  Compiler:         " << r.host.compiler_id << " " << r.host.compiler_version << "\n";
    oss << "  C++ standard:     " << r.host.cxx_standard << "\n";
    oss << "  Git commit:       " << r.host.git_commit << "\n";
    oss << "\n";
    oss << "Target:             HOST_REFERENCE\n";
    oss << "E1:                 NOT CONNECTED\n";
    oss << "E1 toolchain:       "
        << (r.e1_toolchain_found ? ("FOUND (" + r.e1_toolchain_path + ")") : std::string("NOT FOUND")) << "\n";
    oss << "Default crypto backend: " << r.crypto_backend_default << "\n";
    return oss.str();
}

}  // namespace e1cipher::diagnostics

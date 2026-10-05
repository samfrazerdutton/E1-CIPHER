#pragma once
#include <string>

#include "e1cipher/platform/host_platform.hpp"

namespace e1cipher::diagnostics {

/// `e1cipher platform` -- project brief section 32. Useful for an
/// applications engineer debugging a customer deployment: what is this
/// binary actually running on, and is the E1 toolchain present.
struct PlatformReport {
    platform::HostInfo host;
    bool e1_toolchain_found = false;
    std::string e1_toolchain_path;
    std::string crypto_backend_default;
};

[[nodiscard]] PlatformReport build_platform_report();
[[nodiscard]] std::string format_platform_report(const PlatformReport& report);

}  // namespace e1cipher::diagnostics

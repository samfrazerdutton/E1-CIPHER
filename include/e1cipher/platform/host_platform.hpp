#pragma once
#include <string>

namespace e1cipher::platform {

/// Real information about the host this binary is actually running on.
/// Every field here is measured, not asserted -- see src/platform/host/
/// host_platform.cpp. This is what every HOST_REFERENCE benchmark result
/// embeds, mirroring lib/platform/hostInfo.ts's HostInfo from the TS-era
/// prototype.
struct HostInfo {
    std::string os_name;
    std::string arch;
    std::string cpu_model;
    unsigned hardware_threads = 0;
    std::string compiler_id;
    std::string compiler_version;
    std::string cxx_standard;
    std::string git_commit;
    std::string captured_at_iso8601;
};

[[nodiscard]] HostInfo capture_host_info();

}  // namespace e1cipher::platform

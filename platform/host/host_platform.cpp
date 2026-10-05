#include "e1cipher/platform/host_platform.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <thread>

#if defined(_WIN32)
#include <intrin.h>
#endif

namespace e1cipher::platform {

namespace {

std::string cpu_brand_string() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    std::array<int, 4> regs{};
    char brand[0x40] = {};
    __cpuid(regs.data(), static_cast<int>(0x80000000));
    const unsigned n_ext_ids = static_cast<unsigned>(regs[0]);
    if (n_ext_ids >= 0x80000004) {
        for (unsigned i = 0; i < 3; ++i) {
            __cpuid(regs.data(), static_cast<int>(0x80000002 + i));
            std::memcpy(brand + i * 16, regs.data(), sizeof(regs));
        }
        return std::string(brand);
    }
#endif
    return "unknown";
}

std::string os_name() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#elif defined(__APPLE__)
    return "macOS";
#else
    return "unknown";
#endif
}

std::string arch_name() {
#if defined(_M_X64) || defined(__x86_64__)
    return "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#else
    return "unknown";
#endif
}

}  // namespace

HostInfo capture_host_info() {
    HostInfo info;
    info.os_name = os_name();
    info.arch = arch_name();
    info.cpu_model = cpu_brand_string();
    info.hardware_threads = std::thread::hardware_concurrency();
#if defined(__clang__)
    info.compiler_id = "Clang";
    info.compiler_version = __clang_version__;
#elif defined(__GNUC__)
    info.compiler_id = "GCC";
    info.compiler_version = std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#elif defined(_MSC_VER)
    info.compiler_id = "MSVC";
    info.compiler_version = std::to_string(_MSC_VER);
#else
    info.compiler_id = "unknown";
#endif
    info.cxx_standard = std::to_string(__cplusplus);
#if defined(E1CIPHER_GIT_COMMIT)
    info.git_commit = E1CIPHER_GIT_COMMIT;
#else
    info.git_commit = "unknown";
#endif

    const auto now = std::chrono::system_clock::now();
    const auto secs = std::chrono::time_point_cast<std::chrono::seconds>(now);
    const std::time_t t = std::chrono::system_clock::to_time_t(secs);
    std::tm tm_buf{};
#if defined(_WIN32)
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    info.captured_at_iso8601 = buf;

    return info;
}

}  // namespace e1cipher::platform

#pragma once
#include <string_view>

namespace e1cipher::platform {

/// Every measured or reported number in this project is tagged with
/// exactly one of these. There is no "default" — callers must say which
/// one applies. Mirrors lib/platform/e1Target.ts's ResultClass from the
/// TypeScript-era prototype; the discipline carries over unchanged.
enum class ResultClass {
    HostReference,  ///< Measured on ordinary host hardware (this machine).
    RealHardware,   ///< Measured on actual Electron E1 silicon. None exist in this repo.
    Simulated,      ///< An architectural estimate derived from a documented cost model. None exist in this repo (no
                    ///< verified E1 specs to build one from).
    NotMeasured,    ///< Explicitly not measured (e.g. energy — no power instrumentation available).
};

constexpr std::string_view to_string(ResultClass rc) noexcept {
    switch (rc) {
        case ResultClass::HostReference:
            return "HOST_REFERENCE";
        case ResultClass::RealHardware:
            return "REAL_HARDWARE";
        case ResultClass::Simulated:
            return "SIMULATED";
        case ResultClass::NotMeasured:
            return "NOT_MEASURED";
    }
    return "UNKNOWN";
}

}  // namespace e1cipher::platform

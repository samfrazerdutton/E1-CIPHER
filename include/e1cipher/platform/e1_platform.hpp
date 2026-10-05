#pragma once
#include <optional>
#include <string>

#include "e1cipher/platform/result_class.hpp"

namespace e1cipher::platform {

/// Hardware abstraction layer for Efficient Computer's Electron E1.
///
/// STATUS: hardware validation pending. No E1 silicon or effcc toolchain
/// is available in this environment -- see cmake/FindE1Toolchain.cmake,
/// which fails configuration outright if -DTARGET_E1=ON is requested
/// without effcc on PATH, rather than silently falling back to a host
/// build. Every function here either is unavailable or returns a result
/// explicitly tagged ResultClass::NotMeasured. Nothing in this file may be
/// used to fabricate a performance, energy, or architectural number --
/// mirrors lib/platform/e1Target.ts's e1Target from the TS-era prototype.
///
/// Its purpose is to define the integration surface a real E1 backend
/// would need to implement (poly-modulus-sized vector ops, a kernel
/// launch/timing interface) so that benchmarks/crypto and benchmarks/
/// kernels can be retargeted the day hardware or effcc is available,
/// without redesigning the rest of the runtime.
struct E1ExecutionResult {
    ResultClass result_class = ResultClass::NotMeasured;
    std::optional<double> latency_ms;
    std::optional<double> energy_microjoules;
    std::string notes;
};

class E1Platform {
public:
    [[nodiscard]] static bool available() noexcept { return false; }

    /// Always returns NotMeasured with an explanatory note -- see class
    /// comment. There is intentionally no code path in this class that
    /// can produce any other ResultClass.
    [[nodiscard]] static E1ExecutionResult run_kernel(const std::string& kernel_name);
};

}  // namespace e1cipher::platform

#include "e1cipher/platform/e1_platform.hpp"

namespace e1cipher::platform {

E1ExecutionResult E1Platform::run_kernel(const std::string& kernel_name) {
    return E1ExecutionResult{
        ResultClass::NotMeasured,
        std::nullopt,
        std::nullopt,
        "Hardware validation pending: no E1 device or effcc toolchain is available in this "
        "environment to run '" +
            kernel_name + "'. See docs/README.md \"Limitations\".",
    };
}

}  // namespace e1cipher::platform

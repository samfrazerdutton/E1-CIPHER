# Project brief sections 16/49: detect the Electron E1 toolchain (effcc).
# Never silently fall back to a host compiler while claiming an E1 build.
#
# Usage: cmake -DTARGET_E1=ON ..
# If effcc is not found, configuration FAILS with a clear message -- it
# does not continue with a host compiler under an E1 label.

find_program(E1CIPHER_EFFCC_COMPILER effcc)

if(TARGET_E1)
    if(NOT E1CIPHER_EFFCC_COMPILER)
        message(FATAL_ERROR
            "TARGET_E1=ON was requested but the effcc toolchain was not found on PATH.\n"
            "E1 toolchain not found.\n"
            "This project will not silently build a host binary and label it as an E1 build.\n"
            "Install/activate the effcc toolchain, or configure without -DTARGET_E1=ON to build the HOST_REFERENCE target instead.")
    endif()
    message(STATUS "E1 toolchain found: ${E1CIPHER_EFFCC_COMPILER}")
    set(E1CIPHER_PLATFORM_TARGET "E1" CACHE STRING "" FORCE)
else()
    if(E1CIPHER_EFFCC_COMPILER)
        message(STATUS "effcc found at ${E1CIPHER_EFFCC_COMPILER} but TARGET_E1 was not requested -- building HOST_REFERENCE.")
    else()
        message(STATUS "E1 toolchain (effcc): not found. Building HOST_REFERENCE only.")
    endif()
    set(E1CIPHER_PLATFORM_TARGET "HOST" CACHE STRING "" FORCE)
endif()

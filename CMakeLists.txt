cmake_minimum_required(VERSION 3.20)
project(e1cipher LANGUAGES CXX)

# Some fetched third-party projects (doctest v2.4.11) declare a
# cmake_minimum_required() old enough that CMake >= 4.0 refuses to
# configure them at all (compatibility with CMake < 3.5 was removed).
# This tells CMake to apply pre-3.5 policies for those projects rather
# than failing outright -- it does not relax anything about this
# project's own CMakeLists.txt, which requires 3.20 above.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(E1CIPHER_BUILD_TESTS "Build CTest unit/security tests" ON)
option(E1CIPHER_BUILD_BENCHMARKS "Build benchmark executables" ON)
option(TARGET_E1 "Build for the Electron E1 target (requires the effcc toolchain)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(FindE1Toolchain)

# This project is built here with Clang targeting x86_64-pc-windows-msvc
# (no Visual Studio installed -- see docs/architecture-assessment.md).
# Clang defaults to the static CRT (/MT-equivalent) while Microsoft SEAL's
# own CMake build defaults to the dynamic CRT (/MD-equivalent) in Release;
# linking a /MT translation unit against an /MD library fails with
# lld-link's /failifmismatch. Force the whole project (including the
# vendored SEAL build below) onto the dynamic CRT consistently, rather
# than patching around the mismatch per-target.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND WIN32)
    add_compile_options(-fms-runtime-lib=dll)

    # unsigned __int128 division/modulo (used by crypto/kernels/modular.cpp's
    # Montgomery-context math) has no native x86_64 instruction for a full
    # 128-by-128 divide, so Clang emits calls to __udivti3/__umodti3/__divti3.
    # MSVC's own CRT does not provide these (it has no __int128 support at
    # all); they come from Clang's compiler-rt builtins library, which is
    # not linked by default when targeting *-windows-msvc. Locate it via the
    # driver itself (portable across Clang versions/install layouts) rather
    # than hardcoding a path, and link it everywhere.
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} --rtlib=compiler-rt -print-libgcc-file-name
        OUTPUT_VARIABLE E1CIPHER_COMPILER_RT_BUILTINS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(E1CIPHER_COMPILER_RT_BUILTINS AND EXISTS "${E1CIPHER_COMPILER_RT_BUILTINS}")
        message(STATUS "Linking compiler-rt builtins for __int128 support: ${E1CIPHER_COMPILER_RT_BUILTINS}")
        add_link_options("${E1CIPHER_COMPILER_RT_BUILTINS}")
    else()
        message(WARNING "Could not locate Clang's compiler-rt builtins library; "
            "unsigned __int128 division (crypto/kernels/modular.cpp) may fail to link.")
    endif()
endif()

include(FetchContent)

set(SEAL_USE_ZLIB OFF CACHE BOOL "" FORCE)
set(SEAL_USE_ZSTD OFF CACHE BOOL "" FORCE)
set(SEAL_USE_INTEL_HEXL OFF CACHE BOOL "" FORCE)
set(SEAL_BUILD_DEPS ON CACHE BOOL "" FORCE)
set(SEAL_BUILD_SEAL_C OFF CACHE BOOL "" FORCE)
set(SEAL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SEAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SEAL_BUILD_BENCH OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    seal
    GIT_REPOSITORY https://github.com/microsoft/SEAL.git
    GIT_TAG v4.1.2
)
FetchContent_MakeAvailable(seal)

# ---- core library -----------------------------------------------------
file(GLOB_RECURSE E1CIPHER_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/platform/host/*.cpp"
    "${CMAKE_SOURCE_DIR}/platform/e1/*.cpp"
    "${CMAKE_SOURCE_DIR}/platform/mock/*.cpp"
)

# Capture the git commit at configure time (best-effort; falls back to
# "unknown" so a non-git checkout still configures).
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE E1CIPHER_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT E1CIPHER_GIT_COMMIT)
    set(E1CIPHER_GIT_COMMIT "unknown")
endif()

add_library(e1cipher_core STATIC ${E1CIPHER_CORE_SOURCES})
target_include_directories(e1cipher_core PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(e1cipher_core PUBLIC SEAL::seal)
target_compile_definitions(e1cipher_core PUBLIC E1CIPHER_GIT_COMMIT="${E1CIPHER_GIT_COMMIT}")
e1cipher_set_warnings(e1cipher_core)
e1cipher_apply_sanitizers(e1cipher_core)

add_subdirectory(apps)

if(E1CIPHER_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(E1CIPHER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

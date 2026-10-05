# Project brief section 53: AddressSanitizer + UndefinedBehaviorSanitizer
# on host builds. ThreadSanitizer is not enabled by default: it is
# mutually exclusive with ASan in a single binary, and this project's
# threading surface (benchmarks/crypto's thread-scaling benchmark) is
# small enough to sanity-check separately with -DE1CIPHER_SANITIZER=thread
# rather than always-on.
#
# Usage: cmake -DE1CIPHER_SANITIZER=address-undefined ..  (default: none)
set(E1CIPHER_SANITIZER "none" CACHE STRING "Sanitizer to build with: none, address-undefined, thread")
set_property(CACHE E1CIPHER_SANITIZER PROPERTY STRINGS none address-undefined thread)

function(e1cipher_apply_sanitizers target)
    if(E1CIPHER_SANITIZER STREQUAL "address-undefined")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    elseif(E1CIPHER_SANITIZER STREQUAL "thread")
        target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=thread)
    elseif(NOT E1CIPHER_SANITIZER STREQUAL "none")
        message(FATAL_ERROR "Unknown E1CIPHER_SANITIZER: ${E1CIPHER_SANITIZER} (expected none, address-undefined, or thread)")
    endif()
endfunction()

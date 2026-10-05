# Warnings-as-guidance, not warnings-suppressed. Fix the warning rather
# than add a pragma to silence it (project brief section 52).
function(e1cipher_set_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        # SEAL's headers trip -Wignored-pragma-intrinsic under Clang-on-Windows
        # (MSVC-only #pragma intrinsic for _addcarry_u64/_subborrow_u64 that
        # Clang doesn't recognize but falls back correctly for) -- a SEAL
        # upstream/toolchain mismatch, not this project's code, so it is
        # disabled only for targets that include SEAL headers, not globally.
    )
endfunction()

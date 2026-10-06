include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

function(e1cipher_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE e1cipher_core doctest::doctest)
    e1cipher_set_warnings(${name})
    e1cipher_apply_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

e1cipher_add_test(test_kernels unit/test_kernels.cpp)
e1cipher_add_test(test_policy unit/test_policy.cpp)
e1cipher_add_test(test_crypto_roundtrip unit/test_crypto_roundtrip.cpp)
e1cipher_add_test(test_fail_closed security/test_fail_closed.cpp)
e1cipher_add_test(test_runtime unit/test_runtime.cpp)
e1cipher_add_test(test_execution_backend unit/test_execution_backend.cpp)

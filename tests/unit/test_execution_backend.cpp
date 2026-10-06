#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <vector>

#include "e1cipher/runtime/execution_backend.hpp"

using namespace e1cipher::runtime;

TEST_CASE("HostExecutionBackend reports real capabilities, not a fabricated full set") {
    HostExecutionBackend host;
    auto caps = host.query_capabilities();
    CHECK(caps.available);
    // RnsDecompose and KeySwitchInner are deliberately NOT listed -- see
    // execution_backend.cpp's comment: SEAL performs these internally,
    // not as separately callable units, so claiming support would be a
    // fabricated capability.
    const bool lists_rns = std::find(caps.supported_kernels.begin(), caps.supported_kernels.end(),
                                     KernelId::RnsDecompose) != caps.supported_kernels.end();
    CHECK_FALSE(lists_rns);
}

TEST_CASE("HostExecutionBackend actually runs NTT and round-trips via inverse NTT") {
    HostExecutionBackend host;
    auto buf = host.allocate_buffer(16);
    std::vector<std::uint64_t> input(16);
    for (std::size_t i = 0; i < input.size(); ++i) input[i] = i + 1;
    host.copy_to_device(buf, input.data(), input.size());

    host.submit_kernel(KernelId::Ntt, buf);
    host.submit_kernel(KernelId::InverseNtt, buf);

    std::vector<std::uint64_t> output(16);
    host.copy_from_device(output.data(), buf, output.size());
    CHECK(output == input);
}

TEST_CASE("HostExecutionBackend throws (does not silently no-op) for an unsupported kernel") {
    HostExecutionBackend host;
    auto buf = host.allocate_buffer(16);
    CHECK_THROWS_AS(host.submit_kernel(KernelId::RnsDecompose, buf), std::runtime_error);
    CHECK_THROWS_AS(host.submit_kernel(KernelId::KeySwitchInner, buf), std::runtime_error);
}

TEST_CASE("E1ExecutionBackend reports unavailable and never silently falls back to host execution") {
    E1ExecutionBackend e1;
    auto caps = e1.query_capabilities();
    CHECK_FALSE(caps.available);
    CHECK_THROWS_AS((void)e1.allocate_buffer(16), std::runtime_error);

    DeviceBuffer dummy;
    std::vector<std::uint64_t> data(4, 0);
    CHECK_THROWS_AS(e1.copy_to_device(dummy, data.data(), data.size()), std::runtime_error);
    CHECK_THROWS_AS(e1.copy_from_device(data.data(), dummy, data.size()), std::runtime_error);
    CHECK_THROWS_AS(e1.submit_kernel(KernelId::Ntt, dummy), std::runtime_error);
    CHECK_THROWS_AS(e1.synchronize(), std::runtime_error);
}

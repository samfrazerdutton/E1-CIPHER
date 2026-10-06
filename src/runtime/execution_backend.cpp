#include "e1cipher/runtime/execution_backend.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include "e1cipher/crypto/kernels/modular.hpp"
#include "e1cipher/crypto/kernels/ntt.hpp"

namespace e1cipher::runtime {

const char* to_string(KernelId k) noexcept {
    switch (k) {
        case KernelId::Ntt:
            return "NTT";
        case KernelId::InverseNtt:
            return "INVERSE_NTT";
        case KernelId::ModMul:
            return "MOD_MUL";
        case KernelId::RnsDecompose:
            return "RNS_DECOMPOSE";
        case KernelId::KeySwitchInner:
            return "KEY_SWITCH_INNER";
        case KernelId::FeatureAggregate:
            return "FEATURE_AGGREGATE";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------- Host ----

BackendCapabilities HostExecutionBackend::query_capabilities() const {
    return {
        "host-reference", true, {KernelId::Ntt, KernelId::InverseNtt, KernelId::ModMul, KernelId::FeatureAggregate}};
    // RnsDecompose and KeySwitchInner are NOT listed: this reference
    // implementation does not have standalone host kernels for them (SEAL
    // does these internally, not as separately callable units -- see
    // docs/bottleneck-report.md). Listing them here would be a fabricated
    // capability.
}

DeviceBuffer HostExecutionBackend::allocate_buffer(std::size_t element_count) {
    auto storage = std::make_shared<std::vector<std::uint64_t>>(element_count, 0);
    return DeviceBuffer{std::shared_ptr<void>(storage, storage.get()), element_count};
}

void HostExecutionBackend::copy_to_device(DeviceBuffer& dst, const std::uint64_t* src, std::size_t element_count) {
    auto* vec = static_cast<std::vector<std::uint64_t>*>(dst.storage.get());
    if (!vec || element_count > dst.element_count) {
        throw std::runtime_error("HostExecutionBackend::copy_to_device: invalid buffer or size");
    }
    std::memcpy(vec->data(), src, element_count * sizeof(std::uint64_t));
}

void HostExecutionBackend::copy_from_device(std::uint64_t* dst, const DeviceBuffer& src, std::size_t element_count) {
    auto* vec = static_cast<std::vector<std::uint64_t>*>(src.storage.get());
    if (!vec || element_count > src.element_count) {
        throw std::runtime_error("HostExecutionBackend::copy_from_device: invalid buffer or size");
    }
    std::memcpy(dst, vec->data(), element_count * sizeof(std::uint64_t));
}

void HostExecutionBackend::submit_kernel(KernelId kernel, DeviceBuffer& buf) {
    auto* vec = static_cast<std::vector<std::uint64_t>*>(buf.storage.get());
    if (!vec) throw std::runtime_error("HostExecutionBackend::submit_kernel: invalid buffer");

    using namespace crypto::kernels;
    switch (kernel) {
        case KernelId::Ntt: {
            auto result = ntt(*vec, false);
            *vec = std::move(result.coeffs);
            return;
        }
        case KernelId::InverseNtt: {
            auto result = ntt(*vec, true);
            *vec = std::move(result.coeffs);
            return;
        }
        case KernelId::ModMul: {
            // In-place self-multiply mod kNttPrime, as a representative
            // elementwise modular-arithmetic kernel.
            for (auto& x : *vec) x = mod_mul_scalar(x, x, kNttPrime);
            return;
        }
        case KernelId::FeatureAggregate: {
            // Representative reduction kernel: sums the buffer into slot 0.
            std::uint64_t sum = 0;
            for (auto x : *vec) sum = mod_add_scalar(sum, x, kNttPrime);
            if (!vec->empty()) (*vec)[0] = sum;
            return;
        }
        case KernelId::RnsDecompose:
        case KernelId::KeySwitchInner:
            throw std::runtime_error(std::string("HostExecutionBackend: no standalone host kernel for ") +
                                     to_string(kernel) +
                                     " (SEAL performs this internally, not as a separately "
                                     "callable unit -- see docs/bottleneck-report.md)");
    }
}

// ------------------------------------------------------------------ E1 ----

DeviceBuffer E1ExecutionBackend::allocate_buffer(std::size_t) {
    throw std::runtime_error(
        "E1ExecutionBackend::allocate_buffer: E1 hardware/toolchain not available in this "
        "environment. See cmake/FindE1Toolchain.cmake.");
}

void E1ExecutionBackend::copy_to_device(DeviceBuffer&, const std::uint64_t*, std::size_t) {
    throw std::runtime_error("E1ExecutionBackend::copy_to_device: E1 hardware/toolchain not available.");
}

void E1ExecutionBackend::copy_from_device(std::uint64_t*, const DeviceBuffer&, std::size_t) {
    throw std::runtime_error("E1ExecutionBackend::copy_from_device: E1 hardware/toolchain not available.");
}

void E1ExecutionBackend::submit_kernel(KernelId, DeviceBuffer&) {
    throw std::runtime_error(
        "E1ExecutionBackend::submit_kernel: E1 hardware/toolchain not available. This backend "
        "never silently falls back to host execution.");
}

void E1ExecutionBackend::synchronize() {
    throw std::runtime_error("E1ExecutionBackend::synchronize: E1 hardware/toolchain not available.");
}

}  // namespace e1cipher::runtime

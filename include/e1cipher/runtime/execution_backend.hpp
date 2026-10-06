#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace e1cipher::runtime {

/// Candidate kernels an ExecutionBackend may support -- the actual
/// sub-stages of a CKKS key-switch (project brief section 8/9), each a
/// plausible unit of work for a future E1 backend to accelerate.
enum class KernelId {
    Ntt,
    InverseNtt,
    ModMul,
    RnsDecompose,
    KeySwitchInner,
    FeatureAggregate,
};

[[nodiscard]] const char* to_string(KernelId k) noexcept;

/// An opaque "device" buffer handle. For HostExecutionBackend, "device"
/// memory is ordinary host memory; the type is still opaque so calling
/// code never assumes that. Owns its storage via shared_ptr so
/// free_buffer() is optional cleanup, not a use-after-free trap.
struct DeviceBuffer {
    std::shared_ptr<void> storage;
    std::size_t element_count = 0;
};

struct BackendCapabilities {
    std::string device_name;
    bool available = false;
    std::vector<KernelId> supported_kernels;
};

/// The hardware-facing contract (project brief section 9). The host
/// implementation works now; an E1 implementation should fail clearly
/// (throw, not silently fall back to host execution) when the real
/// toolchain/hardware isn't present -- see E1ExecutionBackend below.
class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual BackendCapabilities query_capabilities() const = 0;

    [[nodiscard]] virtual DeviceBuffer allocate_buffer(std::size_t element_count) = 0;
    virtual void copy_to_device(DeviceBuffer& dst, const std::uint64_t* src, std::size_t element_count) = 0;
    virtual void copy_from_device(std::uint64_t* dst, const DeviceBuffer& src, std::size_t element_count) = 0;

    /// Runs `kernel` over `buf` in place. Throws std::runtime_error if the
    /// kernel is unsupported or the device is unavailable -- never a
    /// silent no-op.
    virtual void submit_kernel(KernelId kernel, DeviceBuffer& buf) = 0;
    virtual void synchronize() = 0;
};

/// Runs every supported kernel on ordinary host memory, using the real
/// reference kernels in crypto::kernels -- a genuine, measurable
/// implementation, not a stub. This is what backs runtime::Placement::Accelerated
/// today, and is also the thing CostModel::estimate_accelerated's
/// candidate-kernel timing goes through.
class HostExecutionBackend final : public ExecutionBackend {
public:
    [[nodiscard]] std::string name() const override { return "host-reference"; }
    [[nodiscard]] BackendCapabilities query_capabilities() const override;

    [[nodiscard]] DeviceBuffer allocate_buffer(std::size_t element_count) override;
    void copy_to_device(DeviceBuffer& dst, const std::uint64_t* src, std::size_t element_count) override;
    void copy_from_device(std::uint64_t* dst, const DeviceBuffer& src, std::size_t element_count) override;
    void submit_kernel(KernelId kernel, DeviceBuffer& buf) override;
    void synchronize() override {}  // host execution above is already synchronous
};

/// Electron E1 hardware-facing backend. STATUS: hardware validation
/// pending -- every method either reports unavailability
/// (query_capabilities) or throws std::runtime_error (everything else).
/// There is no code path in this class that can silently execute on the
/// host and claim to be E1. See platform/e1/e1_platform.hpp for the
/// companion (older, result-reporting-focused) stub this complements.
class E1ExecutionBackend final : public ExecutionBackend {
public:
    [[nodiscard]] std::string name() const override { return "e1-candidate (unavailable)"; }
    [[nodiscard]] BackendCapabilities query_capabilities() const override { return {"Electron E1", false, {}}; }

    [[nodiscard]] DeviceBuffer allocate_buffer(std::size_t element_count) override;
    void copy_to_device(DeviceBuffer& dst, const std::uint64_t* src, std::size_t element_count) override;
    void copy_from_device(std::uint64_t* dst, const DeviceBuffer& src, std::size_t element_count) override;
    void submit_kernel(KernelId kernel, DeviceBuffer& buf) override;
    void synchronize() override;
};

}  // namespace e1cipher::runtime

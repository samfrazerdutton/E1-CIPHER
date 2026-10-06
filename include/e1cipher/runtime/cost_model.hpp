#pragma once
#include <memory>

#include "e1cipher/crypto/ckks_backend.hpp"
#include "e1cipher/runtime/types.hpp"

namespace e1cipher::runtime {

/// Produces a CostEstimate per Placement for a given Operation. Every
/// number is tagged with its real provenance (platform::ResultClass) --
/// this class is the one place in the runtime where that discipline is
/// enforced, so callers (Runtime::decide) never have to guess.
///
/// On construction, calibrates LOCAL and ENCRYPTED costs by actually
/// running real work once (a byte-copy proxy for local compute; a real
/// CKKS encrypt+add via crypto::CkksBackend) -- HOST_REFERENCE, not
/// hardcoded constants pretending to be live. ACCELERATED cost runs the
/// real NTT kernel (crypto::kernels::ntt) sized to the operation's vector
/// length as a *candidate-kernel proxy measurement* -- HOST_REFERENCE for
/// the kernel's shape, explicitly not an E1 measurement and not a claim
/// of speedup. REMOTE cost is a bandwidth-time formula --
/// platform::ResultClass::Estimated, never dressed up as measured.
class CostModel {
public:
    /// `param_set` should have key-switching support (see
    /// crypto::ckks_param_sets()); construction calibrates immediately
    /// (real encrypt/add timing), so this is not free -- call once, reuse.
    explicit CostModel(const crypto::CkksParamSet& param_set);

    [[nodiscard]] CostEstimate estimate_local(const Operation& op) const;
    [[nodiscard]] CostEstimate estimate_encrypted(const Operation& op) const;
    [[nodiscard]] CostEstimate estimate_remote(const Operation& op, const HardwareProfile& hw) const;
    [[nodiscard]] CostEstimate estimate_accelerated(const Operation& op) const;

    [[nodiscard]] std::size_t ckks_ciphertext_bytes() const noexcept { return ckks_ciphertext_bytes_; }
    [[nodiscard]] const crypto::CkksParamSet& param_set() const noexcept { return param_set_; }

private:
    const crypto::CkksParamSet& param_set_;
    std::unique_ptr<crypto::CkksBackend> ckks_backend_;

    // Calibrated once at construction, all HOST_REFERENCE.
    double calibrated_local_ns_per_byte_ = 0;
    double calibrated_encrypt_ms_ = 0;
    double calibrated_add_ms_ = 0;
    std::size_t ckks_ciphertext_bytes_ = 0;
};

}  // namespace e1cipher::runtime

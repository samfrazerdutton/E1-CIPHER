#pragma once
#include <memory>
#include <optional>

#include "e1cipher/crypto/backend.hpp"
#include "e1cipher/crypto/ckks_params.hpp"

namespace e1cipher::crypto {

/// Real CKKS backend, backed by Microsoft SEAL (built from source via
/// CMake FetchContent -- see CMakeLists.txt). Uses the pimpl idiom
/// deliberately: <seal/seal.h> is heavy and this header is included by
/// every piece of application code that takes a CryptoBackend&, so SEAL's
/// types are confined to ckks_backend.cpp, not leaked into every caller's
/// include graph. Ports lib/ckks/ckksBackend.ts.
///
/// Construction can fail (SEALContext::parameters_set() == false for an
/// invalid parameter set, or no key-switching support at this N -- see
/// ckks_params.hpp's N1024 note). Use `create()`, not the constructor
/// directly: it returns nullptr on failure rather than throwing, so
/// callers (especially the policy-driven application code, which must
/// fail closed if CONFIDENTIAL data has nowhere to go) can check
/// explicitly instead of needing a try/catch around every construction.
class CkksBackend final : public CryptoBackend {
public:
    ~CkksBackend() override;
    CkksBackend(CkksBackend&&) noexcept;
    CkksBackend& operator=(CkksBackend&&) noexcept;
    CkksBackend(const CkksBackend&) = delete;
    CkksBackend& operator=(const CkksBackend&) = delete;

    [[nodiscard]] static std::unique_ptr<CkksBackend> create(const CkksParamSet& params);

    [[nodiscard]] BackendKind kind() const noexcept override { return BackendKind::Ckks; }
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool is_real_crypto() const noexcept override { return true; }

    [[nodiscard]] EncryptedVector encrypt(const fusion::FeatureVector& input) override;
    [[nodiscard]] EncryptedVector add(const EncryptedVector& a, const EncryptedVector& b) override;
    [[nodiscard]] fusion::FeatureVector decrypt(const EncryptedVector& input) override;

    /// Whether this parameter set has key-switching support (multiply/
    /// relinearize/rescale/rotate). N1024 does not -- see ckks_params.hpp.
    [[nodiscard]] bool supports_key_switching() const noexcept;

    /// Homomorphic multiply + relinearize + rescale, as one op. Not part
    /// of the CryptoBackend interface (add-only aggregation is all the
    /// fleet-gateway application needs) but used by benchmarks/crypto.
    /// Returns std::nullopt if !supports_key_switching().
    [[nodiscard]] std::optional<EncryptedVector> multiply(const EncryptedVector& a, const EncryptedVector& b);

    struct Impl;

private:
    explicit CkksBackend(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace e1cipher::crypto

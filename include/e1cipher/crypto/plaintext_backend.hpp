#pragma once
#include "e1cipher/crypto/backend.hpp"

namespace e1cipher::crypto {

/// Identity backend: no encoding, no encryption, no confidentiality. The
/// zero-crypto baseline every other backend is measured against. Ports
/// lib/ckks/plaintextBackend.ts.
class PlaintextBackend final : public CryptoBackend {
public:
    [[nodiscard]] BackendKind kind() const noexcept override { return BackendKind::Plaintext; }
    [[nodiscard]] std::string name() const override { return "Plaintext"; }
    [[nodiscard]] bool is_real_crypto() const noexcept override { return false; }

    [[nodiscard]] EncryptedVector encrypt(const fusion::FeatureVector& input) override;
    [[nodiscard]] EncryptedVector add(const EncryptedVector& a, const EncryptedVector& b) override;
    [[nodiscard]] fusion::FeatureVector decrypt(const EncryptedVector& input) override;
};

}  // namespace e1cipher::crypto

#pragma once
#include "e1cipher/crypto/backend.hpp"

namespace e1cipher::crypto {

/// NOT CRYPTOGRAPHY. Performs plaintext arithmetic and reports a
/// configurable inflated byte length so the CryptoBackend abstraction has
/// a third, structurally "ciphertext-shaped" implementation for wiring up
/// policy/application code without paying real CKKS cost. Must never be
/// used to produce or stand in for a reported latency/throughput/security
/// number. Ports lib/ckks/mockEncryptedBackend.ts.
class MockBackend final : public CryptoBackend {
public:
    explicit MockBackend(int expansion_factor = 40) : expansion_factor_(expansion_factor) {}

    [[nodiscard]] BackendKind kind() const noexcept override { return BackendKind::Mock; }
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] bool is_real_crypto() const noexcept override { return false; }

    [[nodiscard]] EncryptedVector encrypt(const fusion::FeatureVector& input) override;
    [[nodiscard]] EncryptedVector add(const EncryptedVector& a, const EncryptedVector& b) override;
    [[nodiscard]] fusion::FeatureVector decrypt(const EncryptedVector& input) override;

private:
    int expansion_factor_;
};

}  // namespace e1cipher::crypto

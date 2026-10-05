#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "e1cipher/fusion/feature_vector.hpp"

namespace e1cipher::crypto {

enum class BackendKind { Plaintext, Ckks, Mock };

/// Opaque "encrypted" (or, for Plaintext, merely boxed) payload. Owns its
/// bytes via a std::vector<std::byte> so EncryptedVector is copyable/
/// movable with normal value semantics and no backend-specific lifetime
/// rules leak into calling code — CkksBackend serializes a real SEAL
/// Ciphertext into this on encrypt() and deserializes on every op; that
/// round-trip cost is measured (see benchmarks/crypto), not hidden.
struct EncryptedVector {
    std::vector<std::byte> bytes;

    [[nodiscard]] std::size_t byte_length() const noexcept { return bytes.size(); }
};

/// Abstraction every application/benchmark depends on instead of a
/// concrete crypto library — ports lib/ckks/backend.ts's CryptoBackend
/// interface. Three implementations: PlaintextBackend (zero-crypto
/// baseline), CkksBackend (real Microsoft SEAL), MockBackend (NOT
/// cryptography — see mock_backend.hpp). Swapping backends requires no
/// change to calling code.
class CryptoBackend {
public:
    virtual ~CryptoBackend() = default;

    [[nodiscard]] virtual BackendKind kind() const noexcept = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual bool is_real_crypto() const noexcept = 0;

    [[nodiscard]] virtual EncryptedVector encrypt(const fusion::FeatureVector& input) = 0;
    [[nodiscard]] virtual EncryptedVector add(const EncryptedVector& a, const EncryptedVector& b) = 0;
    [[nodiscard]] virtual fusion::FeatureVector decrypt(const EncryptedVector& input) = 0;
};

using CryptoBackendPtr = std::unique_ptr<CryptoBackend>;

}  // namespace e1cipher::crypto

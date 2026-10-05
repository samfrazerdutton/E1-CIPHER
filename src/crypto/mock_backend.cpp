#include "e1cipher/crypto/mock_backend.hpp"

#include <cstring>

namespace e1cipher::crypto {

// NOT CRYPTOGRAPHY -- see mock_backend.hpp's class comment. This stores
// plaintext bytes plus a padding region sized to *look* like ciphertext
// expansion; the padding is never used for anything and its bytes are
// zero. No reported benchmark number may come from this backend.

std::string MockBackend::name() const { return "MockEncrypted(x" + std::to_string(expansion_factor_) + ")"; }

EncryptedVector MockBackend::encrypt(const fusion::FeatureVector& input) {
    EncryptedVector v;
    const std::size_t real_bytes = sizeof(input.values);
    v.bytes.resize(real_bytes * static_cast<std::size_t>(expansion_factor_), std::byte{0});
    std::memcpy(v.bytes.data(), input.values.data(), real_bytes);
    return v;
}

EncryptedVector MockBackend::add(const EncryptedVector& a, const EncryptedVector& b) {
    fusion::FeatureVector fa = decrypt(a);
    fusion::FeatureVector fb = decrypt(b);
    fusion::FeatureVector sum;
    for (std::size_t i = 0; i < fusion::kFeatureCount; ++i) sum.values[i] = fa.values[i] + fb.values[i];
    return encrypt(sum);
}

fusion::FeatureVector MockBackend::decrypt(const EncryptedVector& input) {
    fusion::FeatureVector v;
    std::memcpy(v.values.data(), input.bytes.data(), sizeof(v.values));
    return v;
}

}  // namespace e1cipher::crypto

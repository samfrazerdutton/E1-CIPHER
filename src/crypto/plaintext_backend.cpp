#include "e1cipher/crypto/plaintext_backend.hpp"

#include <cstring>

namespace e1cipher::crypto {

EncryptedVector PlaintextBackend::encrypt(const fusion::FeatureVector& input) {
    EncryptedVector v;
    v.bytes.resize(sizeof(input.values));
    std::memcpy(v.bytes.data(), input.values.data(), sizeof(input.values));
    return v;
}

EncryptedVector PlaintextBackend::add(const EncryptedVector& a, const EncryptedVector& b) {
    fusion::FeatureVector fa = decrypt(a);
    fusion::FeatureVector fb = decrypt(b);
    fusion::FeatureVector sum;
    for (std::size_t i = 0; i < fusion::kFeatureCount; ++i) sum.values[i] = fa.values[i] + fb.values[i];
    return encrypt(sum);
}

fusion::FeatureVector PlaintextBackend::decrypt(const EncryptedVector& input) {
    fusion::FeatureVector v;
    std::memcpy(v.values.data(), input.bytes.data(), sizeof(v.values));
    return v;
}

}  // namespace e1cipher::crypto

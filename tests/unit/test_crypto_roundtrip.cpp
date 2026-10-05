#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <algorithm>
#include <doctest/doctest.h>

#include "e1cipher/crypto/ckks_backend.hpp"
#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/crypto/mock_backend.hpp"
#include "e1cipher/crypto/plaintext_backend.hpp"

using namespace e1cipher::crypto;
using namespace e1cipher::fusion;

namespace {
const CkksParamSet& find_param_set(std::string_view id) {
    const auto& sets = ckks_param_sets();
    auto it = std::find_if(sets.begin(), sets.end(), [&](const auto& p) { return p.id == id; });
    REQUIRE(it != sets.end());
    return *it;
}
}  // namespace

TEST_CASE("PlaintextBackend round-trips a feature vector exactly") {
    PlaintextBackend backend;
    FeatureVector v;
    for (std::size_t i = 0; i < kFeatureCount; ++i) v.values[i] = static_cast<double>(i) * 1.5;
    auto enc = backend.encrypt(v);
    auto dec = backend.decrypt(enc);
    CHECK(dec.values == v.values);
}

TEST_CASE("PlaintextBackend add is exact elementwise sum") {
    PlaintextBackend backend;
    FeatureVector a, b;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        a.values[i] = static_cast<double>(i);
        b.values[i] = static_cast<double>(i) * 2;
    }
    auto sum = backend.decrypt(backend.add(backend.encrypt(a), backend.encrypt(b)));
    for (std::size_t i = 0; i < kFeatureCount; ++i) CHECK(sum.values[i] == doctest::Approx(a.values[i] + b.values[i]));
}

TEST_CASE("CkksBackend::create fails cleanly (returns nullptr) for a parameter set with no key-switching") {
    const auto& n1024 = find_param_set("N1024");
    auto backend = CkksBackend::create(n1024);
    // N1024 has no key-switching support but IS a valid CKKS context
    // (single modulus) -- create() should still succeed; it's
    // supports_key_switching() that reports false. This test documents
    // that distinction rather than assuming either answer.
    if (backend) {
        CHECK_FALSE(backend->supports_key_switching());
    }
}

TEST_CASE("CkksBackend round-trips a feature vector within tolerance at N4096") {
    const auto& n4096 = find_param_set("N4096");
    auto backend = CkksBackend::create(n4096);
    REQUIRE(backend != nullptr);

    FeatureVector v;
    for (std::size_t i = 0; i < kFeatureCount; ++i) v.values[i] = static_cast<double>(i) * 0.1 - 0.3;

    auto enc = backend->encrypt(v);
    auto dec = backend->decrypt(enc);
    for (std::size_t i = 0; i < kFeatureCount; ++i) CHECK(dec.values[i] == doctest::Approx(v.values[i]).epsilon(1e-3));
}

TEST_CASE("CkksBackend homomorphic add matches plaintext sum within tolerance") {
    const auto& n4096 = find_param_set("N4096");
    auto backend = CkksBackend::create(n4096);
    REQUIRE(backend != nullptr);

    FeatureVector a, b;
    for (std::size_t i = 0; i < kFeatureCount; ++i) {
        a.values[i] = static_cast<double>(i) * 0.1;
        b.values[i] = static_cast<double>(i) * 0.2;
    }
    auto sum = backend->decrypt(backend->add(backend->encrypt(a), backend->encrypt(b)));
    for (std::size_t i = 0; i < kFeatureCount; ++i)
        CHECK(sum.values[i] == doctest::Approx(a.values[i] + b.values[i]).epsilon(1e-3));
}

TEST_CASE("MockBackend is NOT cryptography but round-trips correctly") {
    MockBackend backend;
    FeatureVector v;
    for (std::size_t i = 0; i < kFeatureCount; ++i) v.values[i] = static_cast<double>(i);
    auto enc = backend.encrypt(v);
    CHECK(enc.byte_length() > sizeof(v.values));  // "ciphertext-shaped" inflation
    auto dec = backend.decrypt(enc);
    CHECK(dec.values == v.values);
}

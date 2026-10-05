#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "e1cipher/crypto/kernels/modular.hpp"
#include "e1cipher/crypto/kernels/ntt.hpp"

using namespace e1cipher::crypto::kernels;

namespace {

std::vector<std::uint64_t> naive_cyclic_conv(const std::vector<std::uint64_t>& a, const std::vector<std::uint64_t>& b,
                                             std::uint64_t q) {
    const std::size_t n = a.size();
    std::vector<std::uint64_t> out(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            out[(i + j) % n] = mod_add_scalar(out[(i + j) % n], mod_mul_scalar(a[i], b[j], q), q);
        }
    }
    return out;
}

}  // namespace

TEST_CASE("NTT forward then inverse round-trips") {
    const std::size_t n = 16;
    std::vector<std::uint64_t> a(n);
    for (std::size_t i = 0; i < n; ++i) a[i] = i + 1;
    auto fwd = ntt(a, false);
    auto back = ntt(fwd.coeffs, true);
    CHECK(back.coeffs == a);
}

TEST_CASE("ntt_multiply matches naive O(n^2) cyclic convolution") {
    const std::size_t n = 16;
    std::vector<std::uint64_t> a(n), b(n);
    for (std::size_t i = 0; i < n; ++i) {
        a[i] = i + 1;
        b[i] = 2 * i + 1;
    }
    auto via_ntt = ntt_multiply(a, b);
    auto via_naive = naive_cyclic_conv(a, b, kNttPrime);
    CHECK(via_ntt == via_naive);
}

TEST_CASE("Montgomery multiplication matches naive modular multiplication") {
    MontgomeryContext mont(kNttPrime, 64);
    const std::uint64_t x = 123456789, y = 987654321;
    const std::uint64_t expected = mod_mul_scalar(x, y, kNttPrime);
    const std::uint64_t got = mont.from_montgomery(mont.mul_montgomery(mont.to_montgomery(x), mont.to_montgomery(y)));
    CHECK(got == expected);
}

TEST_CASE("mod_add_scalar wraps correctly at the modulus boundary") {
    CHECK(mod_add_scalar(5, 3, 7) == 1);
    CHECK(mod_add_scalar(0, 0, 7) == 0);
    CHECK(mod_add_scalar(6, 6, 7) == 5);
}

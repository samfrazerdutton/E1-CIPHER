#include "e1cipher/crypto/kernels/ntt.hpp"

#include <bit>
#include <stdexcept>

#include "e1cipher/crypto/kernels/modular.hpp"

namespace e1cipher::crypto::kernels {

namespace {

constexpr std::uint64_t kPrimitiveRoot = 3;

std::uint64_t mod_pow(std::uint64_t base, std::uint64_t exp, std::uint64_t mod) {
    base %= mod;
    std::uint64_t result = 1;
    while (exp > 0) {
        if (exp & 1) result = mod_mul_scalar(result, base, mod);
        base = mod_mul_scalar(base, base, mod);
        exp >>= 1;
    }
    return result;
}

std::uint64_t mod_inverse(std::uint64_t a, std::uint64_t mod) { return mod_pow(a, mod - 2, mod); }

std::uint64_t nth_root_of_unity(std::uint64_t n) {
    const std::uint64_t order = kNttPrime - 1;
    if (order % n != 0) {
        throw std::invalid_argument("NTT_PRIME-1 is not divisible by n; cannot build an n-th root of unity");
    }
    return mod_pow(kPrimitiveRoot, order / n, kNttPrime);
}

std::vector<std::uint64_t> bit_reverse_permute(const std::vector<std::uint64_t>& a) {
    const std::size_t n = a.size();
    const int bits = static_cast<int>(std::bit_width(n)) - 1;
    if ((std::size_t{1} << bits) != n) {
        throw std::invalid_argument("NTT length must be a power of two");
    }
    std::vector<std::uint64_t> out(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t rev = 0;
        for (int b = 0; b < bits; ++b) {
            if (i & (std::size_t{1} << b)) rev |= std::size_t{1} << (bits - 1 - b);
        }
        out[rev] = a[i];
    }
    return out;
}

}  // namespace

NttResult ntt(const std::vector<std::uint64_t>& coeffs, bool inverse) {
    const std::size_t n = coeffs.size();
    std::vector<std::uint64_t> a = bit_reverse_permute(coeffs);
    const std::uint64_t root_n = nth_root_of_unity(static_cast<std::uint64_t>(n));
    const std::uint64_t root = inverse ? mod_inverse(root_n, kNttPrime) : root_n;

    std::uint64_t butterfly_ops = 0;
    int stages = 0;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        ++stages;
        const std::uint64_t w_len = mod_pow(root, n / len, kNttPrime);
        for (std::size_t i = 0; i < n; i += len) {
            std::uint64_t w = 1;
            for (std::size_t j = 0; j < len / 2; ++j) {
                const std::uint64_t u = a[i + j];
                const std::uint64_t v = mod_mul_scalar(a[i + j + len / 2], w, kNttPrime);
                a[i + j] = (u + v) % kNttPrime;
                a[i + j + len / 2] = (u + kNttPrime - v) % kNttPrime;
                w = mod_mul_scalar(w, w_len, kNttPrime);
                ++butterfly_ops;
            }
        }
    }

    if (inverse) {
        const std::uint64_t n_inv = mod_inverse(static_cast<std::uint64_t>(n), kNttPrime);
        for (auto& x : a) x = mod_mul_scalar(x, n_inv, kNttPrime);
    }

    return NttResult{std::move(a), NttStats{butterfly_ops, stages}};
}

std::vector<std::uint64_t> ntt_multiply(const std::vector<std::uint64_t>& a, const std::vector<std::uint64_t>& b) {
    if (a.size() != b.size()) throw std::invalid_argument("operand length mismatch");
    auto [fa, stats_a] = ntt(a, false);
    auto [fb, stats_b] = ntt(b, false);
    std::vector<std::uint64_t> fc(fa.size());
    for (std::size_t i = 0; i < fa.size(); ++i) fc[i] = mod_mul_scalar(fa[i], fb[i], kNttPrime);
    auto [c, stats_c] = ntt(fc, true);
    return c;
}

}  // namespace e1cipher::crypto::kernels

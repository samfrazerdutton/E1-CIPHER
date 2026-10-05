#include "e1cipher/crypto/kernels/modular.hpp"

namespace e1cipher::crypto::kernels {

std::uint64_t mod_mul_scalar(std::uint64_t a, std::uint64_t b, std::uint64_t q) noexcept {
    // unsigned __int128 intermediate: correct for any q that fits in 64
    // bits (every modulus in ckks_params.hpp does) without the overflow
    // risk a naive 64-bit multiply would have. The TS prototype needed
    // BigInt for this; see modular.hpp's class comment.
    const unsigned __int128 product = static_cast<unsigned __int128>(a) * static_cast<unsigned __int128>(b);
    return static_cast<std::uint64_t>(product % q);
}

std::uint64_t mod_add_scalar(std::uint64_t a, std::uint64_t b, std::uint64_t q) noexcept {
    const std::uint64_t s = a + b;
    return s >= q ? s - q : s;
}

std::vector<std::uint64_t> mod_mul_vector_scalar(const std::vector<std::uint64_t>& a,
                                                 const std::vector<std::uint64_t>& b, std::uint64_t q) {
    std::vector<std::uint64_t> out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = mod_mul_scalar(a[i], b[i], q);
    return out;
}

namespace {

std::uint64_t mod_inverse_u64(unsigned __int128 a, unsigned __int128 m) {
    // Extended Euclidean algorithm over __int128 to avoid intermediate
    // overflow; m is a power of two here (R), a is odd.
    using i128 = __int128;
    i128 old_r = static_cast<i128>(a % m);
    i128 r = static_cast<i128>(m);
    i128 old_s = 1, s = 0;
    while (r != 0) {
        i128 quotient = old_r / r;
        i128 tmp_r = old_r - quotient * r;
        old_r = r;
        r = tmp_r;
        i128 tmp_s = old_s - quotient * s;
        old_s = s;
        s = tmp_s;
    }
    i128 result = old_s % static_cast<i128>(m);
    if (result < 0) result += static_cast<i128>(m);
    return static_cast<std::uint64_t>(result);
}

}  // namespace

MontgomeryContext::MontgomeryContext(std::uint64_t q, unsigned r_bits) : q_(q), r_bits_(r_bits) {
    r_ = static_cast<unsigned __int128>(1) << r_bits_;
    // q_inv = -q^-1 mod R
    const unsigned __int128 neg_q_mod_r = r_ - (static_cast<unsigned __int128>(q_) % r_);
    q_inv_ = mod_inverse_u64(neg_q_mod_r, r_);
}

std::uint64_t MontgomeryContext::to_montgomery(std::uint64_t a) const noexcept {
    return static_cast<std::uint64_t>((static_cast<unsigned __int128>(a) << r_bits_) % q_);
}

std::uint64_t MontgomeryContext::redc(unsigned __int128 t) const noexcept {
    const unsigned __int128 mask = r_ - 1;
    const unsigned __int128 m = ((t & mask) * q_inv_) & mask;
    unsigned __int128 result = (t + m * q_) >> r_bits_;
    if (result >= q_) result -= q_;
    return static_cast<std::uint64_t>(result);
}

std::uint64_t MontgomeryContext::from_montgomery(std::uint64_t a_bar) const noexcept { return redc(a_bar); }

std::uint64_t MontgomeryContext::mul_montgomery(std::uint64_t a_bar, std::uint64_t b_bar) const noexcept {
    return redc(static_cast<unsigned __int128>(a_bar) * static_cast<unsigned __int128>(b_bar));
}

std::vector<std::uint64_t> mod_mul_vector_montgomery(const std::vector<std::uint64_t>& a,
                                                     const std::vector<std::uint64_t>& b,
                                                     const MontgomeryContext& ctx) {
    std::vector<std::uint64_t> out(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        const std::uint64_t a_bar = ctx.to_montgomery(a[i]);
        const std::uint64_t b_bar = ctx.to_montgomery(b[i]);
        out[i] = ctx.from_montgomery(ctx.mul_montgomery(a_bar, b_bar));
    }
    return out;
}

}  // namespace e1cipher::crypto::kernels

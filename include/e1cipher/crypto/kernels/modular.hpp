#pragma once
#include <cstdint>
#include <vector>

namespace e1cipher::crypto::kernels {

/// Reference modular-arithmetic kernels, written from scratch in C++ (not
/// calling into SEAL) so their cost can be measured independently of the
/// library -- see benchmarks/kernels and docs/bottleneck-report.md. These
/// are HOST REFERENCE measurements: a scalar baseline, useful for
/// reasoning about where a spatial/dataflow architecture could help, but
/// not a reimplementation of SEAL's internal (Shoup/Barrett-optimized,
/// AVX-vectorized) arithmetic. Ports lib/kernels/modular.ts.
///
/// Uses unsigned __int128 for the intermediate product -- correct and
/// branch-free for any modulus that fits in 64 bits (every modulus in
/// ckks_params.hpp's chains does), unlike the TS prototype, which had to
/// use BigInt because JavaScript has no native 128-bit integer type. This
/// is the first concrete place C++ removes a real constraint the TS
/// version had to work around.
[[nodiscard]] std::uint64_t mod_mul_scalar(std::uint64_t a, std::uint64_t b, std::uint64_t q) noexcept;
[[nodiscard]] std::uint64_t mod_add_scalar(std::uint64_t a, std::uint64_t b, std::uint64_t q) noexcept;

[[nodiscard]] std::vector<std::uint64_t> mod_mul_vector_scalar(const std::vector<std::uint64_t>& a,
                                                               const std::vector<std::uint64_t>& b, std::uint64_t q);

/// Montgomery multiplication context: replaces the modulus-dependent
/// division in mod_mul_scalar with shifts and a precomputed constant.
/// Ports lib/kernels/modular.ts's MontgomeryContext.
class MontgomeryContext {
public:
    explicit MontgomeryContext(std::uint64_t q, unsigned r_bits = 64);

    [[nodiscard]] std::uint64_t to_montgomery(std::uint64_t a) const noexcept;
    [[nodiscard]] std::uint64_t from_montgomery(std::uint64_t a_bar) const noexcept;
    [[nodiscard]] std::uint64_t mul_montgomery(std::uint64_t a_bar, std::uint64_t b_bar) const noexcept;

private:
    [[nodiscard]] std::uint64_t redc(unsigned __int128 t) const noexcept;

    std::uint64_t q_;
    unsigned r_bits_;
    unsigned __int128 r_;
    std::uint64_t q_inv_;  ///< -q^-1 mod R
};

[[nodiscard]] std::vector<std::uint64_t> mod_mul_vector_montgomery(const std::vector<std::uint64_t>& a,
                                                                   const std::vector<std::uint64_t>& b,
                                                                   const MontgomeryContext& ctx);

}  // namespace e1cipher::crypto::kernels

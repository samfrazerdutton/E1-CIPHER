#pragma once
#include <cstdint>
#include <vector>

namespace e1cipher::crypto::kernels {

/// Reference iterative radix-2 Cooley-Tukey NTT / inverse-NTT, written
/// from scratch for kernel-level benchmarking. This is a HOST REFERENCE
/// kernel, not SEAL's internal NTT -- SEAL does not expose its NTT as a
/// standalone callable, so there is no way to time "SEAL's NTT" in
/// isolation. Ports lib/kernels/ntt.ts.
///
/// kNttPrime = 998244353 = 119*2^23+1, primitive root 3 -- a standard
/// NTT-friendly prime supporting lengths up to 2^23, covering every N in
/// ckks_params.hpp.
inline constexpr std::uint64_t kNttPrime = 998244353ULL;

struct NttStats {
    std::uint64_t butterfly_ops = 0;
    int stages = 0;
};

struct NttResult {
    std::vector<std::uint64_t> coeffs;
    NttStats stats;
};

[[nodiscard]] NttResult ntt(const std::vector<std::uint64_t>& coeffs, bool inverse);

/// Cyclic polynomial multiplication mod (X^n - 1) via NTT, for an
/// end-to-end kernel check. RLWE schemes like CKKS use the *negacyclic*
/// ring mod (X^n + 1), which needs a 2n-th root of unity and a twist --
/// this kernel does not implement that twist, so it is a representative
/// NTT workload (same O(N log N) butterfly structure), not a drop-in
/// replacement for SEAL's polynomial multiplication.
[[nodiscard]] std::vector<std::uint64_t> ntt_multiply(const std::vector<std::uint64_t>& a,
                                                      const std::vector<std::uint64_t>& b);

}  // namespace e1cipher::crypto::kernels

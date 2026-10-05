// Kernel micro-benchmarks (project brief section 11/16). Measures
// lib/kernels-equivalent C++ (modular multiply naive vs Montgomery, NTT
// forward) -- HOST REFERENCE, not SEAL internals, not E1 silicon. Ports
// tools/bench/kernel-bench.ts. The TS version found Montgomery *slower*
// than naive BigInt `%` in a JS VM; this C++ port exists specifically to
// check whether that finding was a JS-VM artifact or holds natively too.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

#include "e1cipher/crypto/ckks_params.hpp"
#include "e1cipher/crypto/kernels/modular.hpp"
#include "e1cipher/crypto/kernels/ntt.hpp"
#include "e1cipher/platform/host_platform.hpp"

using namespace e1cipher;
using namespace e1cipher::crypto::kernels;

namespace {

constexpr int kTrials = 20;

template <typename Fn>
double time_op_ms(Fn&& fn, int trials = kTrials) {
    const int warmup = std::max(3, trials / 5);
    for (int i = 0; i < warmup; ++i) fn();
    std::vector<double> samples(static_cast<std::size_t>(trials));
    for (int i = 0; i < trials; ++i) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        samples[static_cast<std::size_t>(i)] =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    return std::accumulate(samples.begin(), samples.end(), 0.0) / trials;
}

std::vector<std::uint64_t> random_vector(std::size_t n, std::uint64_t mod, unsigned seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::uint64_t> dist(0, mod - 1);
    std::vector<std::uint64_t> out(n);
    for (auto& v : out) v = dist(rng);
    return out;
}

}  // namespace

int main() {
    const auto host = platform::capture_host_info();
    std::printf("Kernel benchmark suite (C++)\n");
    std::printf("Host: %s, %s %s\n", host.cpu_model.c_str(), host.compiler_id.c_str(), host.compiler_version.c_str());
    std::printf("HOST REFERENCE RESULTS -- scalar C++ kernels, not SEAL internals, not E1 silicon.\n\n");

    MontgomeryContext mont_ctx(kNttPrime, 64);

    for (const auto& p : crypto::ckks_param_sets()) {
        const std::size_t n = p.poly_modulus_degree;
        auto a = random_vector(n, kNttPrime, 1);
        auto b = random_vector(n, kNttPrime, 2);

        const double naive_ms = time_op_ms([&] { auto r = mod_mul_vector_scalar(a, b, kNttPrime); });
        const double mont_ms = time_op_ms([&] { auto r = mod_mul_vector_montgomery(a, b, mont_ctx); });
        const auto ntt_result = ntt(a, false);
        const double ntt_ms = time_op_ms([&] { auto r = ntt(a, false); });

        std::printf("N=%zu:\n", n);
        std::printf("  modMulVector (naive u128 %%)       mean=%.4fms  (%.0f mults/s)\n", naive_ms,
                    static_cast<double>(n) / naive_ms * 1000.0);
        std::printf("  modMulVector (Montgomery)          mean=%.4fms  (%.0f mults/s)\n", mont_ms,
                    static_cast<double>(n) / mont_ms * 1000.0);
        std::printf("  NTT forward (radix-2, N log N)     mean=%.4fms  butterflyOps=%llu stages=%d\n", ntt_ms,
                    static_cast<unsigned long long>(ntt_result.stats.butterfly_ops), ntt_result.stats.stages);
        std::printf("  Montgomery vs naive: %.2fx\n\n", naive_ms / mont_ms);
    }

    return 0;
}

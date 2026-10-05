#pragma once
#include <cstddef>
#include <string_view>
#include <vector>

namespace e1cipher::crypto {

/// CKKS parameter sets benchmarked by benchmarks/crypto. Ports
/// lib/ckks/paramSets.ts verbatim, including its two documented empirical
/// findings (see notes): N1024 has no room for key-switching at 128-bit
/// security, and N2048's naive 2-prime chain does not work under SEAL's
/// hybrid key-switching (confirmed in the TS prototype; expected to
/// reproduce here since it's a property of the scheme/library, not the
/// host language).
struct CkksParamSet {
    std::string_view id;
    int log_n;
    std::size_t poly_modulus_degree;
    std::vector<int> coeff_modulus_bits;
    int scale_bits;
    std::string_view notes;
};

inline const std::vector<CkksParamSet>& ckks_param_sets() {
    static const std::vector<CkksParamSet> sets = {
        {"N1024",
         10,
         1024,
         {27},
         20,
         "Single-modulus chain (128-bit security budget at N=1024 is only 27 bits). "
         "No room for a second modulus: multiply/relinearize/rescale/rotate are unsupported."},
        {"N2048",
         11,
         2048,
         {18, 17, 18},
         16,
         "Three-prime chain. A naive 2-prime chain ([23,23], sum 46 under the 54-bit budget) "
         "was tried first in the TS prototype but SEAL rejects it at multiply time; 3 smaller "
         "primes are required to get one real multiply+rescale at this N."},
        {"N4096", 12, 4096, {33, 27, 33}, 24, "Three-prime chain. One multiply+relinearize+rescale+rotate verified."},
        {"N8192",
         13,
         8192,
         {60, 40, 40, 60},
         36,
         "Classic SEAL example chain. One multiply+relinearize+rescale+rotate verified."},
        {"N16384",
         14,
         16384,
         {60, 50, 50, 50, 50, 50, 60},
         46,
         "Deep chain. One multiply+relinearize+rescale+rotate verified."},
    };
    return sets;
}

inline int total_coeff_modulus_bits(const CkksParamSet& p) {
    int total = 0;
    for (int b : p.coeff_modulus_bits) total += b;
    return total;
}

}  // namespace e1cipher::crypto

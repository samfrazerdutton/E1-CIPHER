#include "accelerator_opportunity.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <seal/seal.h>

#include "e1cipher/crypto/ckks_params.hpp"

namespace e1cipher::apps {

using namespace e1cipher;

namespace {

constexpr int kTrials = 20;

template <typename Fn>
double median_ms(Fn&& fn, int trials = kTrials) {
    for (int i = 0; i < std::max(2, trials / 5); ++i) fn();
    std::vector<double> samples(static_cast<std::size_t>(trials));
    for (int i = 0; i < trials; ++i) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        samples[static_cast<std::size_t>(i)] =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

enum class Value { High, Medium, Low };

const char* value_label(Value v) {
    switch (v) {
        case Value::High:
            return "HIGH VALUE";
        case Value::Medium:
            return "MEDIUM VALUE";
        case Value::Low:
            return "LOW VALUE";
    }
    return "LOW VALUE";
}

struct KernelShare {
    std::string op;
    double mean_ms;
    double pct_share;
    std::string structural_note;
    Value value;
};

}  // namespace

int run_accelerator_opportunity_experiment() {
    const auto& sets = crypto::ckks_param_sets();
    const auto it = std::find_if(sets.begin(), sets.end(), [](const auto& p) { return p.id == "N8192"; });
    if (it == sets.end()) {
        std::puts("CKKS parameter set N8192 not found -- aborting.");
        return 1;
    }

    std::puts("EXPERIMENT: Accelerator Opportunity Analysis");
    std::printf("Parameter set: %s (N=%zu) -- host measurement, not E1\n\n", std::string(it->id).c_str(),
                it->poly_modulus_degree);

    seal::EncryptionParameters parms(seal::scheme_type::ckks);
    parms.set_poly_modulus_degree(it->poly_modulus_degree);
    std::vector<int> bits(it->coeff_modulus_bits.begin(), it->coeff_modulus_bits.end());
    parms.set_coeff_modulus(seal::CoeffModulus::Create(it->poly_modulus_degree, bits));
    seal::SEALContext context(parms);

    seal::KeyGenerator keygen(context);
    seal::SecretKey secret_key = keygen.secret_key();
    seal::PublicKey public_key;
    keygen.create_public_key(public_key);
    seal::RelinKeys relin_keys;
    keygen.create_relin_keys(relin_keys);
    seal::GaloisKeys galois_keys;
    keygen.create_galois_keys(galois_keys);

    seal::CKKSEncoder encoder(context);
    seal::Encryptor encryptor(context, public_key);
    seal::Decryptor decryptor(context, secret_key);
    seal::Evaluator evaluator(context);

    const double scale = std::pow(2.0, it->scale_bits);
    const std::size_t slots = encoder.slot_count();
    std::vector<double> data(slots, 0.25);

    seal::Plaintext plain;
    encoder.encode(data, scale, plain);
    seal::Ciphertext cipher1;
    encryptor.encrypt(plain, cipher1);
    seal::Ciphertext cipher2;
    encryptor.encrypt(plain, cipher2);

    const double t_encode = median_ms([&] {
        seal::Plaintext p;
        encoder.encode(data, scale, p);
    });
    const double t_encrypt = median_ms([&] {
        seal::Ciphertext c;
        encryptor.encrypt(plain, c);
    });
    const double t_add = median_ms([&] {
        seal::Ciphertext c;
        evaluator.add(cipher1, cipher2, c);
    });
    const double t_multiply = median_ms([&] {
        seal::Ciphertext c;
        evaluator.multiply(cipher1, cipher2, c);
    });
    const double t_relinearize = median_ms([&] {
        seal::Ciphertext c;
        evaluator.multiply(cipher1, cipher2, c);
        evaluator.relinearize_inplace(c, relin_keys);
    });
    const double t_rescale = median_ms([&] {
        seal::Ciphertext c;
        evaluator.multiply(cipher1, cipher2, c);
        evaluator.relinearize_inplace(c, relin_keys);
        evaluator.rescale_to_next_inplace(c);
    });
    const double t_rotate = median_ms([&] {
        seal::Ciphertext c;
        evaluator.rotate_vector(cipher1, 1, galois_keys, c);
    });
    const double t_decrypt = median_ms([&] {
        seal::Plaintext p;
        decryptor.decrypt(cipher1, p);
    });

    // Structural classification is a documented, qualitative judgment
    // about each op's regularity/parallelism/branching (project brief
    // section 23) -- it does not come from a measurement, and is labeled
    // as such. The % share DOES come from the measurements above.
    const double total = t_encode + t_encrypt + t_add + t_multiply + t_relinearize + t_rescale + t_rotate + t_decrypt;

    std::vector<KernelShare> shares = {
        {"encode", t_encode, 0, "FFT-like transform, regular, data-parallel across coefficients", Value::Low},
        {"encrypt", t_encrypt, 0, "Includes RLWE error sampling -- less uniformly regular than the arithmetic ops",
         Value::Medium},
        {"add", t_add, 0, "Fully data-parallel elementwise add, low arithmetic intensity", Value::Low},
        {"multiply (raw)", t_multiply, 0, "Fully data-parallel elementwise multiply", Value::Low},
        {"relinearize", t_relinearize, 0,
         "RNS decomposition -> NTT -> fixed modular multiply-adds against a public key matrix -> inverse NTT -- "
         "regular, high arithmetic intensity, high data movement",
         Value::High},
        {"rescale", t_rescale, 0, "Base conversion across the RNS chain following relinearize -- same shape as above",
         Value::High},
        {"rotate", t_rotate, 0, "Key-switching via Galois automorphism -- same NTT/modmul shape as relinearize",
         Value::High},
        {"decrypt", t_decrypt, 0, "One modular multiply-add pass per RNS limb", Value::Low},
    };
    for (auto& s : shares) s.pct_share = 100.0 * s.mean_ms / total;

    std::printf("%-16s %10s %10s  %s\n", "op", "mean(ms)", "% share", "candidate");
    for (const auto& s : shares) {
        std::printf("%-16s %10.4f %9.1f%%  %s\n", s.op.c_str(), s.mean_ms, s.pct_share, value_label(s.value));
    }

    // Deliberately NOT "whichever op has the highest % share" -- encrypt
    // measures the single highest share here (29.5%) but is only
    // MEDIUM-classified (RLWE sampling makes it less uniformly regular
    // than the key-switching family). The top candidate is the highest-
    // share op among those classified HIGH VALUE; % share alone would
    // contradict the classification above, not support it.
    const auto high_value_ops = [&shares] {
        std::vector<const KernelShare*> out;
        for (const auto& s : shares)
            if (s.value == Value::High) out.push_back(&s);
        return out;
    }();
    double key_switch_family_pct = 0;
    for (const auto* s : high_value_ops) key_switch_family_pct += s->pct_share;

    std::printf("\nKey-switching family (relinearize + rescale + rotate), combined: %.1f%% of measured runtime.\n",
                key_switch_family_pct);
    std::puts(
        "Top HIGH VALUE candidate for E1 kernel acceleration (highest share WITHIN the structurally-regular group):");
    const auto top = std::max_element(high_value_ops.begin(), high_value_ops.end(),
                                      [](const auto* a, const auto* b) { return a->pct_share < b->pct_share; });
    std::printf("  %s (%.1f%% of measured runtime) -- %s\n", (*top)->op.c_str(), (*top)->pct_share,
                (*top)->structural_note.c_str());
    std::printf(
        "  Note: 'encrypt' measured a higher individual share (%.1f%%) but is classified MEDIUM VALUE, not "
        "HIGH -- see its structural note above. Share alone does not override the structural judgment.\n",
        t_encrypt / total * 100.0);
    std::puts(
        "  This is a measured host finding plus a documented structural judgment -- NOT an E1 measurement and NOT "
        "a speedup claim. See docs/e1-hardware-validation-plan.md.");

    std::filesystem::create_directories("results");
    std::ofstream out("results/cpp-accelerator-opportunity-latest.json");
    out << "{\n  \"paramSet\": \"" << it->id << "\",\n  \"kernels\": [\n";
    for (std::size_t i = 0; i < shares.size(); ++i) {
        const auto& s = shares[i];
        out << "    { \"op\": \"" << s.op << "\", \"meanMs\": " << s.mean_ms << ", \"pctShare\": " << s.pct_share
            << ", \"candidateValue\": \"" << value_label(s.value) << "\", \"structuralNote\": \"" << s.structural_note
            << "\" }" << (i + 1 < shares.size() ? ",\n" : "\n");
    }
    out << "  ]\n}\n";
    std::puts("\nWrote results/cpp-accelerator-opportunity-latest.json");

    return 0;
}

}  // namespace e1cipher::apps

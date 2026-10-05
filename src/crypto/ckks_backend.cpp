#include "e1cipher/crypto/ckks_backend.hpp"

#include <cmath>
#include <cstring>
#include <sstream>

#include <seal/seal.h>

namespace e1cipher::crypto {

struct CkksBackend::Impl {
    seal::SEALContext context;
    seal::SecretKey secret_key;
    seal::PublicKey public_key;
    seal::RelinKeys relin_keys;
    seal::CKKSEncoder encoder;
    seal::Encryptor encryptor;
    seal::Decryptor decryptor;
    seal::Evaluator evaluator;
    double scale;
    bool has_key_switching;
    std::string param_id;
};

CkksBackend::CkksBackend(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
CkksBackend::~CkksBackend() = default;
CkksBackend::CkksBackend(CkksBackend&&) noexcept = default;
CkksBackend& CkksBackend::operator=(CkksBackend&&) noexcept = default;

std::unique_ptr<CkksBackend> CkksBackend::create(const CkksParamSet& params) {
    seal::EncryptionParameters parms(seal::scheme_type::ckks);
    parms.set_poly_modulus_degree(params.poly_modulus_degree);

    std::vector<int> bits(params.coeff_modulus_bits.begin(), params.coeff_modulus_bits.end());
    parms.set_coeff_modulus(seal::CoeffModulus::Create(params.poly_modulus_degree, bits));

    seal::SEALContext context(parms);
    if (!context.parameters_set()) {
        return nullptr;
    }

    seal::KeyGenerator keygen(context);
    seal::SecretKey secret_key = keygen.secret_key();
    seal::PublicKey public_key;
    keygen.create_public_key(public_key);

    const bool has_ks = context.using_keyswitching();
    seal::RelinKeys relin_keys;
    if (has_ks) {
        keygen.create_relin_keys(relin_keys);
    }

    // Not std::make_unique<Impl>(Impl{...}): that forwards through a copy-
    // construction of the temporary Impl, and SEAL's objects (CKKSEncoder
    // et al.) are move-only (their internal util::Pointer deletes its copy
    // constructor) -- Impl is therefore move-only too. Aggregate-`new` it
    // directly so each member is direct-initialized (move, for the rvalues
    // below) in place instead.
    std::unique_ptr<Impl> impl(new Impl{
        context,
        secret_key,
        public_key,
        relin_keys,
        seal::CKKSEncoder(context),
        seal::Encryptor(context, public_key),
        seal::Decryptor(context, secret_key),
        seal::Evaluator(context),
        std::pow(2.0, params.scale_bits),
        has_ks,
        std::string(params.id),
    });

    return std::unique_ptr<CkksBackend>(new CkksBackend(std::move(impl)));
}

std::string CkksBackend::name() const { return "CKKS(" + impl_->param_id + ")"; }

bool CkksBackend::supports_key_switching() const noexcept { return impl_->has_key_switching; }

namespace {

/// Serializes a seal::Ciphertext into an EncryptedVector's byte buffer.
/// This round-trip (object -> bytes -> object on every op) is a real,
/// measured cost -- see benchmarks/crypto -- not hidden behind the
/// CryptoBackend interface's convenience.
EncryptedVector pack(const seal::Ciphertext& c) {
    std::ostringstream oss(std::ios::binary);
    c.save(oss, seal::compr_mode_type::none);
    const std::string s = oss.str();
    EncryptedVector v;
    v.bytes.resize(s.size());
    std::memcpy(v.bytes.data(), s.data(), s.size());
    return v;
}

seal::Ciphertext unpack(const seal::SEALContext& context, const EncryptedVector& v) {
    seal::Ciphertext c;
    std::istringstream iss(std::string(reinterpret_cast<const char*>(v.bytes.data()), v.bytes.size()),
                           std::ios::binary);
    c.load(context, iss);
    return c;
}

}  // namespace

EncryptedVector CkksBackend::encrypt(const fusion::FeatureVector& input) {
    std::vector<double> values(input.values.begin(), input.values.end());
    seal::Plaintext plain;
    impl_->encoder.encode(values, impl_->scale, plain);
    seal::Ciphertext cipher;
    impl_->encryptor.encrypt(plain, cipher);
    return pack(cipher);
}

EncryptedVector CkksBackend::add(const EncryptedVector& a, const EncryptedVector& b) {
    seal::Ciphertext ca = unpack(impl_->context, a);
    seal::Ciphertext cb = unpack(impl_->context, b);
    seal::Ciphertext out;
    impl_->evaluator.add(ca, cb, out);
    return pack(out);
}

fusion::FeatureVector CkksBackend::decrypt(const EncryptedVector& input) {
    seal::Ciphertext cipher = unpack(impl_->context, input);
    seal::Plaintext plain;
    impl_->decryptor.decrypt(cipher, plain);
    std::vector<double> decoded;
    impl_->encoder.decode(plain, decoded);

    fusion::FeatureVector v;
    for (std::size_t i = 0; i < fusion::kFeatureCount && i < decoded.size(); ++i) {
        v.values[i] = decoded[i];
    }
    return v;
}

std::optional<EncryptedVector> CkksBackend::multiply(const EncryptedVector& a, const EncryptedVector& b) {
    if (!impl_->has_key_switching) return std::nullopt;
    seal::Ciphertext ca = unpack(impl_->context, a);
    seal::Ciphertext cb = unpack(impl_->context, b);
    seal::Ciphertext out;
    impl_->evaluator.multiply(ca, cb, out);
    impl_->evaluator.relinearize_inplace(out, impl_->relin_keys);
    impl_->evaluator.rescale_to_next_inplace(out);
    return pack(out);
}

}  // namespace e1cipher::crypto

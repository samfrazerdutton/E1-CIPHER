#pragma once
#include <cstdint>

#include "e1cipher/telemetry/types.hpp"

namespace e1cipher::apps {

struct FleetOptions {
    std::uint32_t seed = 7;
    telemetry::Scenario scenario = telemetry::Scenario::Normal;
    std::uint32_t tick = 0;
};

/// `e1cipher fleet` -- project brief section 22. Runs plaintext vs CKKS
/// aggregation across several fleet sizes. The gateway role in this
/// function never constructs a Decryptor or holds a SecretKey -- it only
/// ever touches crypto::EncryptedVector objects and CryptoBackend::add();
/// decrypt() is called by a separate "analysis endpoint" role further
/// down in the same function, modeling the trust boundary described in
/// docs/threat-model.md even though both roles happen to run in the same
/// process here (see the printed output, which labels which role does what).
int run_fleet(const FleetOptions& opts);

}  // namespace e1cipher::apps

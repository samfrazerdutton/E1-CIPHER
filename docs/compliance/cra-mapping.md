# Cyber Resilience Act (CRA) Mapping

**This document maps engineering artifacts this repository generates to
the kinds of evidence a CRA risk assessment would ask for. It is not a
compliance claim, a legal opinion, or a certification. A GitHub repository
cannot certify a commercial product — only a real conformity assessment
process, performed by people qualified to do one, can.**

## What the CRA is (non-legal summary)

The EU Cyber Resilience Act sets cybersecurity requirements for products
with digital elements sold in the EU — secure-by-design/default
development, vulnerability handling throughout the support period, and
specific technical documentation (an SBOM, a risk assessment, evidence of
testing). It applies to commercial products placed on the market, not to
research prototypes or internal tools — this repository is the latter.

## What's relevant to a product built on this codebase

If this runtime were the basis of an actual shipped product (e.g. a
drone's onboard confidential-compute stack), the CRA-relevant engineering
work would include: a real vulnerability-handling process, a signed
update mechanism, a maintained SBOM across the product's support period,
and documented evidence of the security testing performed. None of that
exists here as an operating capability — this document exists to name the
gap precisely, not to gesture past it.

## What this repository actually generates

| Evidence category | Where | Real or template? |
|---|---|---|
| Asset inventory | `docs/cra-evidence/01-asset-inventory.md` (TS-era, Node tooling) | Real — generated from the repo's actual file tree |
| Dependency inventory | `docs/cra-evidence/02-dependency-inventory.md` + this C++ project's `CMakeLists.txt` `FetchContent` declarations (SEAL v4.1.2, doctest v2.4.11, pinned exact tags) | Real, but split across two tool ecosystems — see `docs/architecture-assessment.md`'s note that the Node evidence generator does not yet enumerate C++ dependencies |
| SBOM | `results/sbom.json` (CycloneDX, Node `package.json` only) | Real for the Node tooling layer; **no CycloneDX/SPDX SBOM is generated for the C++ dependency tree** — a real, named gap |
| Cryptographic inventory | `docs/cra-evidence/03-cryptographic-configuration-record.md` (TS-era) + `include/e1cipher/crypto/ckks_params.hpp` (C++, current) | Real |
| Vulnerability register | `docs/cra-evidence/04-vulnerability-register.md` (real `npm audit` scan of the Node tooling layer only) | Real for Node deps; **no vulnerability scan is run against SEAL/doctest's C++ dependency tree** |
| Security update policy | `docs/cra-evidence/05-security-update-policy.md` | States plainly: none implemented |
| Support period | `docs/cra-evidence/06-support-period.md` | States plainly: none committed |
| Incident response | `docs/cra-evidence/07-incident-response-procedure.md` | Explicitly labeled a draft template, not an operating capability |
| Security architecture / threat model | `docs/threat-model.md` | Real — 10 threats, explicit MITIGATED/PARTIALLY MITIGATED/NOT MITIGATED/OUT OF SCOPE status each |
| Attack surface map | `docs/cra-evidence/10-attack-surface-map.md` (TS-era; does not yet cover the C++ CLI's own surface) | Real but incomplete — a named gap |
| Security test results | `e1cipher security` (this C++ project, live) + `ctest` output + `docs/cra-evidence/09-security-test-results.md` (TS-era) | Real |
| Build provenance | Git commit embedded via `E1CIPHER_GIT_COMMIT` (`CMakeLists.txt`), compiler/flags visible via `e1cipher platform` | Real |
| Data-flow / trust-boundary diagram | `docs/cra-evidence/11-data-flow-and-trust-boundary-diagram.md` (TS-era) + `apps/fleet_gateway/fleet.hpp`'s documented role separation (C++, current) | Real |

## Explicitly out of scope / unimplemented

- No signed release pipeline, no code-signing, no OTA update mechanism.
- No CVE monitoring/subscription against SEAL or any other dependency.
- No formal risk assessment performed by a qualified assessor.
- No SBOM covering the C++ dependency tree (SEAL, msgsl, doctest) — only
  the Node tooling layer has one.
- No legal review of whether or how the CRA would apply to any real
  product derived from this code.

## How to use this document

Point to it when asked "is this CRA compliant?" The honest answer is: no,
this is a research prototype, and compliance is a property of a shipped
product and a conformity-assessment process, not of a GitHub repository.
What this repository *does* demonstrate is the kind of engineering
evidence — threat modeling, fail-closed security tests, build
provenance, a (partial) SBOM — that a real compliance effort would build
on top of.

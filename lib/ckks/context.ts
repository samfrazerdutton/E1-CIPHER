import type { MainModule, SEALContext, Encryptor, Decryptor, Evaluator, CKKSEncoder, RelinKeys, GaloisKeys, SecretKey, PublicKey } from 'node-seal';
import { getSeal } from './seal.js';
import type { CkksParamSet } from './paramSets.js';

export interface CkksContext {
  readonly paramSet: CkksParamSet;
  readonly seal: MainModule;
  readonly context: SEALContext;
  readonly parametersSet: boolean;
  readonly maxBitsAt128: number;
  readonly usingKeyswitching: boolean;
  readonly slotCount: number;
  readonly encoder: CKKSEncoder;
  readonly encryptor: Encryptor;
  readonly decryptor: Decryptor;
  readonly evaluator: Evaluator;
  readonly secretKey: SecretKey;
  readonly publicKey: PublicKey;
  /** undefined when the chain has no room for key-switching (e.g. a single-modulus parameter set). */
  readonly relinKeys: RelinKeys | undefined;
  readonly galoisKeys: GaloisKeys | undefined;
}

/**
 * Builds a full CKKS context (keys, encoder, encryptor/decryptor, evaluator)
 * for a given parameter set. Does NOT throw when the parameter set fails to
 * reach 128-bit security, or when it has no room for key-switching (relin/
 * rotation) — callers must check `parametersSet` / `usingKeyswitching` and
 * handle the gap explicitly (see tools/bench/ckks-bench.ts), per the project
 * rule that unsupported configurations must be reported, not hidden.
 */
export async function buildCkksContext(paramSet: CkksParamSet): Promise<CkksContext> {
  const seal = await getSeal();

  const maxBitsAt128 = seal.CoeffModulus.MaxBitCount(paramSet.polyModulusDegree, seal.SecLevelType.tc128);

  const parms = new seal.EncryptionParameters(seal.SchemeType.ckks);
  parms.setPolyModulusDegree(paramSet.polyModulusDegree);
  parms.setCoeffModulus(
    seal.CoeffModulus.Create(paramSet.polyModulusDegree, Int32Array.from(paramSet.coeffModulusBits)),
  );

  const context = new seal.SEALContext(parms, true, seal.SecLevelType.tc128);
  const parametersSet = context.parametersSet();
  const usingKeyswitching = parametersSet && context.usingKeyswitching();

  const keyGenerator = new seal.KeyGenerator(context);
  const secretKey = keyGenerator.secretKey();
  const publicKey = keyGenerator.createPublicKey();
  const relinKeys = usingKeyswitching ? keyGenerator.createRelinKeys() : undefined;
  const galoisKeys = usingKeyswitching ? keyGenerator.createGaloisKeys() : undefined;

  const encoder = new seal.CKKSEncoder(context);
  const encryptor = new seal.Encryptor(context, publicKey);
  const decryptor = new seal.Decryptor(context, secretKey);
  const evaluator = new seal.Evaluator(context);

  return {
    paramSet,
    seal,
    context,
    parametersSet,
    maxBitsAt128,
    usingKeyswitching,
    slotCount: parametersSet ? encoder.slotCount() : 0,
    encoder,
    encryptor,
    decryptor,
    evaluator,
    secretKey,
    publicKey,
    relinKeys,
    galoisKeys,
  };
}

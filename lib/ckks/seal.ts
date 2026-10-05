import SEAL from 'node-seal';
import type { MainModule } from 'node-seal';

let sealInstance: Promise<MainModule> | undefined;

/**
 * node-seal loads a WASM build of Microsoft SEAL and exposes raw Emscripten
 * bindings. Initialization is expensive (WASM instantiation), so the module
 * is loaded once per process and shared by every caller.
 */
export function getSeal(): Promise<MainModule> {
  if (!sealInstance) {
    sealInstance = SEAL();
  }
  return sealInstance;
}

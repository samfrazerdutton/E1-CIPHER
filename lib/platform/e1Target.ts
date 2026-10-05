/**
 * Hardware abstraction layer for Efficient Computer's Electron E1.
 *
 * STATUS: Hardware validation pending. No E1 silicon or effcc toolchain was
 * available while building this project. Every function here either throws
 * or returns a result explicitly tagged `resultClass: 'NOT_MEASURED'` —
 * nothing in this file may be used to fabricate a performance, energy, or
 * compiler-mapping number. Its purpose is to define the integration surface
 * (what a real E1 backend would need to implement) so the kernels in
 * lib/kernels and the benchmarks in tools/bench can be retargeted the day
 * hardware or the compiler is available, without redesigning the rest of
 * the system.
 */

export type ResultClass = 'REAL_HARDWARE' | 'HOST_REFERENCE' | 'SIMULATED_E1' | 'NOT_MEASURED';

export interface E1ExecutionResult {
  readonly resultClass: ResultClass;
  readonly latencyMs: number | null;
  readonly energyMicrojoules: number | null;
  readonly notes: string;
}

export interface E1Target {
  readonly name: string;
  readonly available: boolean;
  runKernel(kernelName: string): Promise<E1ExecutionResult>;
}

export const e1Target: E1Target = {
  name: 'Electron E1 (effcc toolchain)',
  available: false,
  async runKernel(kernelName: string): Promise<E1ExecutionResult> {
    return {
      resultClass: 'NOT_MEASURED',
      latencyMs: null,
      energyMicrojoules: null,
      notes:
        `Hardware validation pending: no E1 device or effcc toolchain is available in this ` +
        `environment to run '${kernelName}'. See docs/README.md "Limitations".`,
    };
  },
};

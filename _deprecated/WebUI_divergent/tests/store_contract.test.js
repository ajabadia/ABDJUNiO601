import { describe, it, expect } from 'vitest';
import { PARAMETER_REGISTRY, PARAMETER_MAP, normalizeValue, denormalizeValue } from '../js/registry.gen.js';

describe('WebUI Parameter Registry & Contract Tests', () => {
  it('should have valid parameter definitions', () => {
    expect(PARAMETER_REGISTRY.length).toBeGreaterThan(0);
    expect(PARAMETER_MAP['vcf_cutoff']).toBeDefined();
    expect(PARAMETER_MAP['vcf_cutoff'].min).toBe(0.0);
    expect(PARAMETER_MAP['vcf_cutoff'].max).toBe(1.0);
  });

  it('should correctly normalize values', () => {
    const norm = normalizeValue('vcf_cutoff', 0.5);
    expect(norm).toBe(0.5);
  });

  it('should correctly denormalize float values', () => {
    const raw = denormalizeValue('vcf_cutoff', 0.5);
    expect(raw).toBe(0.5);
  });

  it('should correctly handle integer parameters (e.g. chorus_mode)', () => {
    const raw = denormalizeValue('chorus_mode', 0.66); // max = 3 -> 0.66 * 3 = 1.98 -> rounded 2
    expect(raw).toBe(2);
  });
});

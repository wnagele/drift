import { describe, test, expect } from 'vitest';

import {
  CHECKSUM_FAILED,
  CHECKSUM_PASSED,
  CHECKSUM_SKIPPED,
  EASA_MEMBER_STATE_CODES,
  joinOperatorInput,
  luhnMod36Checksum,
  sanitize,
  splitOperatorInput,
  validateChecksum,
  validateStructure,
} from '../registration.js';

// EASA's own worked example from AMC1/GM1 to Article 14(6): the full
// registration string FIN87astrdge12k8-xyz, i.e. country FIN, random part
// 87astrdge12k, checksum 8, secret digits xyz.
const EASA_RANDOM = '87astrdge12k';
const EASA_SECRET = 'xyz';
const EASA_NUMBER = `FIN${EASA_RANDOM}8`;

describe('luhnMod36Checksum', () => {
  test("reproduces EASA's worked example", () => {
    // The single assertion that pins the whole algorithm against a primary
    // source. The country code is deliberately NOT part of the checksum
    // input - only the 12 random characters plus the 3 secret digits.
    expect(luhnMod36Checksum(EASA_RANDOM + EASA_SECRET)).toEqual('8');
  });
});

describe('sanitize', () => {
  test('strips both official renderings of the separators', () => {
    // EASA hyphenates the full string; the CAA renders theirs with spaces.
    // A user pasting either must not be told their number is malformed.
    expect(sanitize('FIN87astrdge12k8-xyz')).toEqual('FIN87astrdge12k8xyz');
    expect(sanitize('GBR gc284pmztcrt l 2ot')).toEqual('GBRgc284pmztcrtl2ot');
    expect(sanitize(undefined)).toEqual('');
  });
});

describe('splitOperatorInput', () => {
  test('splits the public number from the secret digits', () => {
    expect(splitOperatorInput('FIN87astrdge12k8-xyz'))
      .toEqual({ number: EASA_NUMBER, secret: EASA_SECRET });
  });

  test('yields an empty secret when only the number was entered', () => {
    expect(splitOperatorInput(EASA_NUMBER)).toEqual({ number: EASA_NUMBER, secret: '' });
  });

  test('round-trips through joinOperatorInput', () => {
    const { number, secret } = splitOperatorInput('FIN87astrdge12k8-xyz');
    expect(joinOperatorInput(number, secret)).toEqual('FIN87astrdge12k8-xyz');
    // With no secret there is no separator to render.
    expect(joinOperatorInput(number, '')).toEqual(EASA_NUMBER);
    expect(joinOperatorInput('', '')).toEqual('');
  });
});

describe('validateStructure', () => {
  test('accepts a well-formed EU number', () => {
    expect(validateStructure(EASA_NUMBER, 'EU').valid).toBe(true);
  });

  test('rejects anything that is not 16 characters after sanitizing', () => {
    expect(validateStructure('FIN87astrdge12k', 'EU').valid).toBe(false);
    // The full 20-character string is not a valid *number* - it has to be
    // split first, which is what splitOperatorInput is for.
    expect(validateStructure('FIN87astrdge12k8xyz', 'EU').valid).toBe(false);
  });

  test('accepts the four EFTA member states, not just the EU 27', () => {
    // These are in scope under Article 129 of Regulation (EU) 2018/1139.
    // Because the syntax check *blocks* saving, omitting them would make
    // DRIFT unconfigurable for a Norwegian or Swiss operator.
    for (const code of ['NOR', 'ISL', 'LIE', 'CHE']) {
      const result = validateStructure(`${code}drift0test01h`, 'EU');
      expect(result.valid, code).toBe(true);
    }
    expect(EASA_MEMBER_STATE_CODES).toHaveLength(31);
  });

  test('rejects a country code outside the EASA member states', () => {
    // JPN and CHN are the two regions deliberately out of scope, so a
    // contributor wiring them up trips this.
    for (const code of ['XXX', 'USA', 'JPN', 'CHN']) {
      expect(validateStructure(`${code}drift0test01h`, 'EU').valid, code).toBe(false);
    }
  });

  test('holds the EU and UK registries apart', () => {
    // The UK runs its own CAA registry; values are not interchangeable.
    expect(validateStructure('GBRgc284pmztcrth', 'UK').valid).toBe(true);
    expect(validateStructure('GBRgc284pmztcrth', 'EU').valid).toBe(false);
    expect(validateStructure('AUTdrift0test01h', 'UK').valid).toBe(false);
  });

  test('rejects non-alphanumeric body characters', () => {
    expect(validateStructure('AUTdrift0test0!h', 'EU').valid).toBe(false);
    expect(validateStructure('AU1drift0test01h', 'EU').valid).toBe(false);
  });
});

describe('validateChecksum', () => {
  test('passes a matching registration number and verification code', () => {
    expect(validateChecksum(EASA_NUMBER, EASA_SECRET).status).toEqual(CHECKSUM_PASSED);
  });

  test('fails a mismatched verification code', () => {
    expect(validateChecksum(EASA_NUMBER, 'abc').status).toEqual(CHECKSUM_FAILED);
  });

  test('skips rather than fails when no verification code was supplied', () => {
    // The load-bearing case: most national registries appear not to issue
    // the secret digits at all, so "absent" is the common, compliant state
    // and must never read as a failure.
    expect(validateChecksum(EASA_NUMBER, '').status).toEqual(CHECKSUM_SKIPPED);
    expect(validateChecksum(EASA_NUMBER, undefined).status).toEqual(CHECKSUM_SKIPPED);
    // Whitespace-only sanitizes down to nothing, i.e. also "not provided".
    expect(validateChecksum(EASA_NUMBER, '   ').status).toEqual(CHECKSUM_SKIPPED);
  });

  test('fails a verification code of the wrong length or charset', () => {
    expect(validateChecksum(EASA_NUMBER, 'xy').status).toEqual(CHECKSUM_FAILED);
    expect(validateChecksum(EASA_NUMBER, 'xyzw').status).toEqual(CHECKSUM_FAILED);
    expect(validateChecksum(EASA_NUMBER, 'xy!').status).toEqual(CHECKSUM_FAILED);
  });
});

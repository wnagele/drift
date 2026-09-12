// EU/UK operator registration number validation.
//
// The algorithm is verified against EASA's own worked example
// (FIN87astrdge12k8-xyz) by registration.test.js. These rules live here
// rather than in the firmware on purpose: none of the three regions differs
// in wire encoding, schedule or transport, so region only decides which
// config inputs are required and how they are validated. See the region note
// in AGENTS.md.
//
// Sources: EASA Easy Access Rules for UAS, AMC1/GM1 to Article 14(6) and
// AMC1 Article 14(8) of Implementing Regulation (EU) 2019/947.

// ISO 3166-1 alpha-3 codes of the 31 EASA member states: the EU 27 plus the
// four EFTA states (Iceland, Liechtenstein, Norway, Switzerland) that
// participate under Article 129 of Regulation (EU) 2018/1139 and are subject
// to the same drone rules. The EFTA four are included deliberately - a
// 27-code EU-only list would hard-reject a valid Norwegian or Swiss
// registration number, and the syntax check blocks saving. Note EASA
// membership establishes that the rules apply to these states; it is not
// itself confirmation that each national registry issues alpha-3-prefixed
// numbers in this format.
export const EASA_MEMBER_STATE_CODES = [
  'AUT', 'BEL', 'BGR', 'HRV', 'CYP', 'CZE', 'DNK', 'EST', 'FIN', 'FRA',
  'DEU', 'GRC', 'HUN', 'IRL', 'ITA', 'LVA', 'LTU', 'LUX', 'MLT', 'NLD',
  'POL', 'PRT', 'ROU', 'SVK', 'SVN', 'ESP', 'SWE',
  'ISL', 'LIE', 'NOR', 'CHE',
];

// The UK runs its own CAA registry with a fixed prefix; its values are not
// interchangeable with any EASA member state's.
export const UK_COUNTRY_CODE = 'GBR';

// Luhn mod-36 code points: digits 0-9 map to 0-9, then a-z to 10-35.
const ALPHABET = '0123456789abcdefghijklmnopqrstuvwxyz';

export const REGISTRATION_NUMBER_LENGTH = 16;
export const SECRET_DIGITS_LENGTH = 3;

const codePointOf = (ch) => {
  const index = ALPHABET.indexOf(ch.toLowerCase());
  if (index < 0) throw new RangeError(`character '${ch}' is not alphanumeric`);
  return index;
};

// Strip whitespace and dashes before anything else looks at the input. Both
// official renderings carry separators - EASA's full registration string
// uses a hyphen ("FIN87astrdge12k8-xyz"), the CAA renders theirs with spaces
// ("GBR gc284pmztcrt l 2ot") - so a user pasting either must not be told
// their own registration number is malformed.
export const sanitize = (raw) => (raw || '').replace(/[\s-]/g, '');

const luhnMod36CheckDigit = (codePoints, n = 36) => {
  let factor = 2;
  let sum = 0;
  for (let i = codePoints.length - 1; i >= 0; i--) {
    let addend = factor * codePoints[i];
    factor = factor === 2 ? 1 : 2;
    addend = Math.floor(addend / n) + (addend % n);
    sum += addend;
  }
  return (n - (sum % n)) % n;
};

export const luhnMod36Checksum = (payload) =>
  ALPHABET[luhnMod36CheckDigit(payload.split('').map(codePointOf))];

// Split a sanitized operator input into its public and secret halves. This
// is what lets the user paste the whole 20-character registration string
// into one field: the leading 16 characters are the registration number
// that gets broadcast, anything beyond them is the verification code, which
// is stored separately and never goes on the wire. Without this, a pasted
// full string would fit the 20-character ODID_ID_SIZE field exactly and the
// private key would be broadcast - which the CAA explicitly warns against.
export function splitOperatorInput(raw) {
  const sanitized = sanitize(raw);
  return {
    number: sanitized.slice(0, REGISTRATION_NUMBER_LENGTH),
    secret: sanitized.slice(REGISTRATION_NUMBER_LENGTH),
  };
}

// Rejoin the two stored halves for display, in EASA's own rendering.
export function joinOperatorInput(number, secret) {
  return secret ? `${number}-${secret}` : (number || '');
}

export function parseRegistrationNumber(regNumber) {
  const sanitized = sanitize(regNumber);
  if (sanitized.length !== REGISTRATION_NUMBER_LENGTH) return null;
  return {
    countryCode: sanitized.slice(0, 3),
    randomPart: sanitized.slice(3, 15),
    checksumChar: sanitized.slice(15, 16),
  };
}

// Structural validation - the only check possible without the secret
// digits: length, country-code prefix and character classes.
export function validateStructure(regNumber, region) {
  const sanitized = sanitize(regNumber);
  if (sanitized.length !== REGISTRATION_NUMBER_LENGTH) {
    return {
      valid: false,
      error: `Expected ${REGISTRATION_NUMBER_LENGTH} characters, got ${sanitized.length}.`,
    };
  }

  const parsed = parseRegistrationNumber(sanitized);
  if (!/^[A-Za-z]{3}$/.test(parsed.countryCode)) {
    return { valid: false, error: `'${parsed.countryCode}' is not a 3-letter country code.` };
  }

  const countryCode = parsed.countryCode.toUpperCase();
  if (region === 'UK') {
    if (countryCode !== UK_COUNTRY_CODE) {
      return { valid: false, error: `UK numbers start with ${UK_COUNTRY_CODE}, not '${countryCode}'.` };
    }
  } else if (!EASA_MEMBER_STATE_CODES.includes(countryCode)) {
    return { valid: false, error: `'${countryCode}' is not an EASA member state country code.` };
  }

  if (!/^[a-z0-9]{12}$/i.test(parsed.randomPart)) {
    return { valid: false, error: 'Characters 4-15 must be alphanumeric.' };
  }
  if (!/^[a-z0-9]$/i.test(parsed.checksumChar)) {
    return { valid: false, error: 'The last character must be alphanumeric.' };
  }
  return { valid: true, parsed };
}

// The three states of the checksum check. "skipped" is not a failure: most
// national registries appear not to issue the secret digits at all (see
// AGENTS.md), so rendering a missing code as an error would show the
// majority of compliant operators a red cross.
export const CHECKSUM_SKIPPED = 'skipped';
export const CHECKSUM_PASSED = 'passed';
export const CHECKSUM_FAILED = 'failed';

// Cryptographic consistency check, per AMC1 Article 14(6) points (c)-(d):
// checksum = Luhn-mod-36(randomPart + secretDigits). The 3-letter country
// code is excluded from the checksum input entirely.
export function validateChecksum(regNumber, secretDigits) {
  const secret = sanitize(secretDigits);
  if (!secret) return { status: CHECKSUM_SKIPPED };

  if (secret.length !== SECRET_DIGITS_LENGTH || !/^[a-z0-9]+$/i.test(secret)) {
    return {
      status: CHECKSUM_FAILED,
      error: `The verification code must be ${SECRET_DIGITS_LENGTH} alphanumeric characters.`,
    };
  }

  const parsed = parseRegistrationNumber(regNumber);
  if (!parsed) return { status: CHECKSUM_SKIPPED };

  let expected;
  try {
    expected = luhnMod36Checksum(parsed.randomPart.toLowerCase() + secret.toLowerCase());
  } catch {
    return { status: CHECKSUM_SKIPPED };
  }
  if (expected !== parsed.checksumChar.toLowerCase()) {
    return {
      status: CHECKSUM_FAILED,
      error: 'The registration number and verification code do not match.',
    };
  }
  return { status: CHECKSUM_PASSED };
}

// What the Config form calls: takes the single free-text operator input,
// splits it, and reports both checks.
export function validateOperatorInput(raw, region) {
  const { number, secret } = splitOperatorInput(raw);
  return {
    number,
    secret,
    structure: validateStructure(number, region),
    checksum: validateChecksum(number, secret),
  };
}

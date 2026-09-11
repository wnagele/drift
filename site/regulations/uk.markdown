---
layout: page
title: United Kingdom
parent: Regulations
nav_order: 3
permalink: /regulations/uk/
---
# United Kingdom

The UK retained the EU's 2019/945 and 2019/947 regulations post-Brexit and has since
amended them on its own timeline via **The Unmanned Aircraft (Amendment) Regulations
2025 (S.I. 2025/1106)**. The underlying field lists are, at the time of writing,
unedited carry-overs from the EU text — the UK only substituted "CAA" for "national
competent authority" and "UK-class" for "C-class" labels — but the applicability
dates diverge substantially from the EU's.

## Applicability

- Same product-category split as the EU: **Part 6** of the retained 2019/945 Annex
  covers a retrofit add-on (DRIFT's category); **Part 2** (and by the same pattern
  Parts 3/4/16/17) covers built-in DRI on factory-built, class-marked UK1/UK2/UK3/UK5/UK6
  aircraft.
- Class marks were renamed **UK0–UK6** (from the EU's C0–C6) by S.I. 2025/1106 — a
  labelling change; the UK now runs its own domestic product-standards framework
  rather than relying on EU harmonised standards.

## Timeline

This is the one area where the UK diverges sharply from the EU, and where DRIFT's
actual retrofit-add-on use case is **not yet under a live mandate**:

| Date | Event |
|:---|:---|
| 2025-09 | CAA opens voluntary early adoption of Direct RID, inviting feedback |
| 2026-01-01 | Direct RID mandate begins — **for class-marked UK1/UK2/UK3/UK5/UK6 aircraft only** |
| 2027-12-31 | Last day EU C-class-marked aircraft may fly in the UK "as if" the equivalent UK class |
| 2028-01-01 | Mandate extends to UK0, UK4 model aircraft (unless exempted), privately-built UAS ≥100 g with a camera, and **legacy UAS outside UK class marking ≥100 g with a camera — the actual retrofit/add-on scenario** |

([CAA, CAP3172 "Remote ID in the UK"](https://www.caa.co.uk/cap3172);
[CAA, "Remote ID (RID)" consumer page](https://www.caa.co.uk/drones/open-category/moving-on-to-more-advanced-flying/remote-id-rid/);
[The Unmanned Aircraft (Amendment) Regulations 2025](https://www.legislation.gov.uk/uksi/2025/1106/note/made))

The CAA regards Direct RID as an **interim** measure pending a preferred long-term
"Hybrid RID" (Direct + Networked) approach.

## Required messages & fields

Identical in substance to the EU Part 6 list (see the [European
Union](/regulations/eu/) page for the ASTM message/field mapping): operator
registration number, add-on serial number, time stamp, UA position and height,
route/course and ground speed, and remote-pilot-or-take-off position. No emergency
status field.

([legislation.gov.uk, retained 2019/945, Annex Part 6](https://www.legislation.gov.uk/eur/2019/945/annex/part/6))

## Transports

Same pattern as the EU: no Bluetooth/Wi-Fi/PHY named in the regulation text itself —
just "an open and documented transmission protocol."

Post-Brexit, the UK has its own standard-designation mechanism (retained 2019/945,
[Article 3A](https://www.legislation.gov.uk/eur/2019/945/article/3A)), letting the
Secretary of State designate BSI-adopted standards in place of the EU's "harmonised
standards." Whether EN 4709-002 has actually been designated under Article 3A could
not be confirmed against a primary UK source — flagged here as unverified; industry
expectation is that the UK will accept the same ASD-STAN/ASTM-F3411-compatible
broadcast already implemented for the EU.

## Identity & registration fields

| You provide | Goes on the wire as | Format | Where to get it |
|:---|:---|:---|:---|
| UK "Remote ID number" | Operator ID message, `OperatorId` | Same structure as the EU scheme this was carried over from (see [European Union](/regulations/eu/#the-registration-number-in-detail)): a public registration number plus a separately-issued private/secret part. CAA's own illustrative example shows the checksum character as `7/b`, i.e. "could be either a digit or a letter" — using the actual Luhn mod-36 checksum computed for the rest of that example payload instead: `GBR gc284pmztcrt l-2ot` (checksum `l`, self-consistent and verified against the EASA AMC1 Article 14(6) algorithm) | CAA drone operator registration — separate from any EU member state's registry, not interchangeable with one |
| Add-on serial number | Basic ID message, `UASID` | ANSI/CTA-2063-A-2019 | Assigned by the add-on's manufacturer |

{: .important }
> A UK Remote ID number also has a separately-held 3-character **private key**,
> which CAA explicitly warns must **never** be entered where it would be broadcast
> ("Never write your private key on your drone"). Only the public part goes on the
> wire.

## Not required

- **Emergency status** — omitted from the operative Part 6 text, same as the EU. CAA's public-facing consumer guidance describes emergency status as part of "what Remote ID transmits" generally without distinguishing add-on vs. built-in, so treat this as legally optional for an add-on, while practically expected once the wider ecosystem catches up.
- **UK-specific class-marking logic in the add-on itself** — class marks are a concern for whichever airframe the add-on is fitted to, not something the add-on asserts.

## Sources

- [CAA — "Remote ID (RID)" policy programme](https://www.caa.co.uk/drones/drone-regulations/policy-programmes/remote-id-rid/)
- [CAA — "Remote ID (RID)" consumer page](https://www.caa.co.uk/drones/open-category/moving-on-to-more-advanced-flying/remote-id-rid/)
- [CAA — CAP3172 "Remote ID in the UK"](https://www.caa.co.uk/cap3172)
- [CAA — "Class marks" page](https://www.caa.co.uk/drones/open-category/getting-started-with-drones-and-model-aircraft/class-marks/)
- [legislation.gov.uk — retained Regulation (EU) 2019/945, Annex Part 6](https://www.legislation.gov.uk/eur/2019/945/annex/part/6)
- [legislation.gov.uk — The Unmanned Aircraft (Amendment) Regulations 2025 (S.I. 2025/1106)](https://www.legislation.gov.uk/uksi/2025/1106/note/made)

Last verified 2026-09-10.

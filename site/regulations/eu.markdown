---
layout: page
title: European Union
parent: Regulations
nav_order: 2
permalink: /regulations/eu/
---
# European Union

The EU calls this **Direct Remote Identification (DRI)** — "the local broadcast of
information about an unmanned aircraft in operation... so that this information can
be obtained without physical access to the unmanned aircraft"
([Implementing Regulation (EU) 2019/947, Article 2(13)](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:02019R0947-20250501)).
DRIFT is a retrofit accessory, not a factory-built class-marked aircraft, so the
controlling technical text is **Part 6 of the Annex to Commission Delegated
Regulation (EU) 2019/945**, "Requirements for a direct remote identification
add-on" — its own product category, legally distinct from the C0–C6 class-marking
Parts 1–5/16/17.

## Applicability

- Part 6 applies to a standalone DRI add-on placed on the market as its own product,
  independent of any specific airframe or class ([Article 2(1)(b)](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:02019R0945-20250624)).
- Fitting a Part 6 add-on to an airframe does **not** confer any C0–C6 class marking
  on that airframe — the add-on's compliance and the airframe's class status are
  independent questions.
- Class marks (C1–C3) carry their own, built-in DRI requirement (Parts 2–4) with a
  different, larger field list (it adds emergency status) — not the add-on path
  DRIFT implements.

## Timeline

| Date | Event |
|:---|:---|
| 2019 | Delegated Regulation (EU) 2019/945 and Implementing Regulation (EU) 2019/947 adopted |
| — | Delegated Regulation (EU) 2020/1058 amends Part 6: adds the time-stamp requirement and the verification-code wording — texts based on the original, un-amended 2019 OJ version are out of date |
| 2023-12-31 / 2024-01-01 | Legacy-UAS mitigation/transition windows for non-class-marked "open"-category flying end ([Implementing Regulation (EU) 2019/947, Articles 20 & 22](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:02019R0947-20250501), as last amended by (EU) 2022/425) — both dates have already lapsed, with no further extension in the current consolidated text |
| 2024-08-01 | Commission Implementing Decision (EU) 2024/2103 publishes the **EN 4709-002:2023** reference, with a stated restriction (see Transports below) |

## Required messages & fields

Per the current, amended Part 6 text:

| Requirement | ASTM message | Field | Notes |
|:---|:---|:---|:---|
| UAS operator registration number | Operator ID | `OperatorId` | See [Identity & registration fields](#identity--registration-fields) below — a 16-character structured registration number; DRIFT always validates its structure, and additionally validates the real checksum if the user also has and provides the separately-issued verification code |
| Add-on's own unique serial number, ANSI/CTA-2063-A-2019 | Basic ID | `IDType = Serial Number`, `UASID` | |
| Time stamp | Location/Vector | `TimeStamp` | Added by the 2020/1058 amendment — missing from the original 2019 text |
| Geographical position of the UA | Location/Vector | `Latitude`, `Longitude` | |
| Height above the surface or take-off point | Location/Vector | `Height`, `HeightType = Over takeoff` | |
| Route/course (from true north) | Location/Vector | `Direction` | Track/course-over-ground, not aircraft heading/yaw |
| Ground speed | Location/Vector | `SpeedHorizontal` | |
| Geographical position of the remote pilot, or if unavailable, the take-off point | System | `OperatorLatitude`, `OperatorLongitude`, `OperatorAltitudeGeo`, `OperatorLocationType` | Same conditional fallback structure as the US rule — take-off point is always an acceptable value |

([Regulation (EU) 2019/945, Annex Part 6, point 3](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:02019R0945-20250624))

## Transports

Part 6 is transport-agnostic: it requires only "an open and documented transmission
protocol... received directly by existing mobile devices within the broadcasting
range" — no Bluetooth/Wi-Fi/PHY is named in the regulation text itself.

The harmonised standard is **EN 4709-002:2023**, referenced (with a restriction) by
[Commission Implementing Decision (EU) 2024/2103](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32024D2103):
the decision states the standard does **not** adequately cover the "received by
existing mobile devices" requirement on its own, so following it alone does not give
a full presumption of conformity on that specific point — additional interoperability
evidence is expected.

There is no EU network-RID alternative for add-ons: network Remote ID is defined only
as an *optional* feature for class-marked aircraft (Parts 2–4), and Part 6 does not
mention it at all. Direct/local broadcast is the only mechanism available to a Part 6
product.

{: .note }
> No numeric rate/latency/range figures appear in the regulation text — Part 6 only
> requires broadcasting "in real time during the whole duration of the flight."
> Those numbers live inside EN 4709-002:2023, which is paywalled; DRIFT's schedule
> (Location ~4 Hz, statics 1–2 Hz) is understood to clear the ASTM F3411 baseline the
> standard is built on, but the EN's own numeric table has not been independently
> verified against primary text.

## Identity & registration fields

| You provide | Goes on the wire as | Format | Where to get it |
|:---|:---|:---|:---|
| UAS operator registration number | Operator ID message, `OperatorId` | 16 characters: 3-letter ISO 3166 alpha-3 country code + 12 random alphanumerics + 1 checksum character | Issued at UAS operator registration ([Article 14, Implementing Regulation (EU) 2019/947](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:02019R0947-20250501)) |
| Add-on serial number | Basic ID message, `UASID` | ANSI/CTA-2063-A-2019 | Assigned by the add-on's manufacturer |

### The registration number, in detail

EASA's own technical specification for this scheme — [Easy Access Rules for
Unmanned Aircraft Systems, AMC1/GM1 to Article 14(6) and AMC1 Article
14(8)](https://www.easa.europa.eu/en/document-library/easy-access-rules/online-publications/easy-access-rules-unmanned-aircraft-systems)
of Implementing Regulation (EU) 2019/947 — defines the exact structure:

- The **registration number** is 16 characters: a 3-letter country code, 12 random
  lowercase alphanumerics, and a trailing checksum character.
- The registration system separately generates **3 "secret digits"** — this is the
  "verification code"/"additional number" Part 6 refers to.
- The checksum embedded in the 16-character number is computed (Luhn mod-36) over
  the 12 random characters *plus* the 3 secret digits — so the checksum can't be
  validated by anyone who only sees the visible 16-character number. That's the
  entire point of the scheme: it stops someone from photographing your drone's
  registration sticker and reusing/forging it.
- At registration, EASA's scheme envisions you receiving a **"full registration
  string"**: the 16-character number + `-` + the 3 secret digits — 20 characters
  total, matching `ODID_ID_SIZE` in the vendored ODID library exactly. EASA's own
  worked example: `FIN87astrdge12k8-xyz`.
- [AMC1 Article 14(8)](https://www.easa.europa.eu/en/document-library/easy-access-rules/online-publications/easy-access-rules-unmanned-aircraft-systems)
  envisions displaying only the **16-character number** on the drone's physical
  sticker, with the full 20-character string (including the secret digits)
  uploaded into the Remote ID device.

{: .important }
> **AMC/GM material is EASA's "Acceptable Means of Compliance" guidance, not a
> hard mandate on national registration systems** — it describes *an* acceptable
> way to satisfy Part 6's consistency-check requirement, and member states can
> implement the requirement differently. In practice, at least one member state's
> registration process (Austria) issues operators **only the 16-character
> registration number, with no separate secret-digits code at all** — real-world,
> first-hand evidence, though anecdotal and not independently verified across
> other member states. This tracks with the secret-digits scheme reading as
> "considered in EASA's guidance but not uniformly implemented nationally."
>
> Given that, DRIFT validates the registration number in **two layers**,
> depending on what the user actually has:
>
> 1. **Structural validation (always run):** exactly 16 characters, first 3
>    characters matching a known EU member state's ISO 3166 alpha-3 code,
>    remaining 13 characters alphanumeric. This is the only thing verifiable
>    when a user has just the registration number, which real-world evidence
>    suggests is common.
> 2. **Checksum validation (opportunistic):** if the user also has and
>    provides the 3-character verification code, DRIFT additionally runs the
>    real Luhn mod-36 checksum defined in EASA AMC1 Article 14(6) point (c)
>    (computed over the registration number's 12 random characters plus the
>    verification code) and flags a mismatch. If the verification code isn't
>    provided, this layer is skipped rather than treated as an error —
>    structural validation alone is sufficient to proceed.
>
> Either way, only the 16-character registration number is ever broadcast —
> the verification code, when supplied, is used purely for the local
> consistency check and never goes on the wire. Whitespace and dashes are
> stripped from both inputs before either layer runs, so a registration
> number or verification code pasted with EASA's hyphen formatting or any
> other spacing still validates correctly.

## Not required

- **Emergency status** — required for built-in DRI on class-marked C1–C3 aircraft, but **absent from the Part 6 add-on field list** entirely. This is the inverse of the naive assumption that add-ons mirror built-in systems.
- **EU category/class fields** (`CategoryEU`/`ClassEU`) — these apply only to factory-built, class-marked aircraft (C0–C6), not to a Part 6 add-on, which carries no class label at all.
- **Network Remote ID** — not defined or available for the add-on product category.

## Sources

- [Regulation (EU) 2019/945 (consolidated)](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:02019R0945-20250624)
- [Implementing Regulation (EU) 2019/947 (consolidated)](https://eur-lex.europa.eu/legal-content/EN/TXT/HTML/?uri=CELEX:02019R0947-20250501)
- [Commission Delegated Regulation (EU) 2020/1058](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32020R1058) (added the Part 6 verification-code/consistency-check language)
- [EASA — Easy Access Rules for Unmanned Aircraft Systems](https://www.easa.europa.eu/en/document-library/easy-access-rules/online-publications/easy-access-rules-unmanned-aircraft-systems), AMC1/GM1 to Article 14(6), AMC1 Article 14(8)
- [Commission Implementing Decision (EU) 2024/2103](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32024D2103)

Last verified 2026-09-11.

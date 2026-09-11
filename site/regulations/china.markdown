---
layout: page
title: China
parent: Regulations
nav_order: 7
permalink: /regulations/china/
---
# China

China has an in-force Remote ID standard, but it is a **separate, genuinely
incompatible wire protocol** — not a variant of ASTM F3411/ASD-STAN. DRIFT does not
and cannot implement it without a second, from-scratch encoder stack.

## Status

**GB 46750-2025** — 《民用无人驾驶航空器系统运行识别规范》 ("Civil Unmanned Aircraft System
Operation Identification Specification") — was published 2025-10-31 and became
effective **2026-05-01**. A weight-tiered mandate applies: RPA 250 g – 25 kg require
Broadcast Remote ID; RPA above 25 kg require Network Remote ID (per Transport
Canada's own regulatory text, cited as a secondary corroboration of an operative
Chinese mandate — see [Canada](/regulations/canada/)).

## Why it's incompatible

The upstream `opendroneid-core-c` project (which DRIFT vendors its ASTM/ODID encoder
from) maintains a dedicated, separate `libopendroneidcn` module for China's format,
confirming the two protocols are not interchangeable:

- A flag-bitmask chain (Type/Version/DataLen/Flags/Data) instead of ASTM F3411's six
  fixed 25-byte message types.
- **Mandatory dual identity** — a 20-character serial number *and* an 8-character
  UIN registration ID — rather than ASTM's serial-*or*-session-ID choice.
- No authentication message.
- **Mandatory** ground-control-station/operator position and accuracy-category
  fields that ASTM F3411 leaves optional or undefined.
- Support for both WGS-84 and CGCS2000 coordinate systems.

## Sources

- [opendroneid-core-c — `libopendroneidcn` README](https://github.com/opendroneid/opendroneid-core-c/blob/master/libopendroneidcn/README.md)
- Transport Canada, NPA 2026-005, §3.4 (secondary corroboration of the operative mandate)

Last verified 2026-09-10. The exact GB 46750-2025 title/date was cross-checked
against a secondary aggregator, not an independently retrieved SAMR/CAAC primary
document — treat the title transliteration and precise dates as best-current-
knowledge rather than fully primary-sourced.

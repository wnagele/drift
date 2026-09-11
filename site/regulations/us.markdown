---
layout: page
title: United States
parent: Regulations
nav_order: 1
permalink: /regulations/us/
---
# United States

The FAA's Remote ID rule ([14 CFR Part 89](https://www.ecfr.gov/current/title-14/part-89))
defines three ways to comply: a Standard Remote ID aircraft, flying inside an
FAA-Recognized Identification Area (FRIA), or fitting a **remote identification
broadcast module** — the category DRIFT falls into. Broadcast-only operation with no
network/internet connection is fully sufficient; Part 89 has no network-RID operating
requirement.

## Applicability

- Applies to unmanned aircraft required to be registered under Part 89 that are not
  otherwise Standard Remote ID-equipped and not flown exclusively within a FRIA.
- A broadcast module attaches to an existing aircraft; it does not need to be built
  in, and fitting one does not require the aircraft to meet any other Standard RID
  equipment requirement.
- No network Remote ID alternative exists in the US rule — broadcast (or FRIA, or
  Standard RID) are the only three paths.

([14 CFR §89.115(a)](https://www.ecfr.gov/current/title-14/section-89.115))

## Timeline

| Date | Event |
|:---|:---|
| 2021-04-21 | Broadcast modules may not be produced unless they meet an FAA-accepted Means of Compliance ([§89.520](https://www.ecfr.gov/current/title-14/section-89.520)) |
| 2022-08-11 | FAA accepts **ASTM F3586-22** as a Means of Compliance ([87 FR 49520](https://www.federalregister.gov/documents/2022/08/11/2022-16997/accepted-means-of-compliance-remote-identification-of-unmanned)) |
| 2022-09-16 | Standard RID production deadline (enforcement discretion on broadcast-module production noncompliance through 2022-12-16, [87 FR 55685](https://www.federalregister.gov/documents/2022/09/12/2022-19644/enforcement-policy-regarding-production-requirements-for-standard-remote-identification-unmanned)) |
| 2023-09-16 | Operator compliance deadline ([§89.105](https://www.ecfr.gov/current/title-14/section-89.105)); enforcement discretion through 2024-03-16 ([88 FR 63518](https://www.federalregister.gov/documents/2023/09/15/2023-20074/enforcement-policy-regarding-operator-compliance-deadline-for-remote-identification-of-unmanned)) |

## Required messages & fields

§89.315 lists the complete, exhaustive set a broadcast module must be capable of
broadcasting — this maps directly onto ASTM F3411's Basic ID, Location/Vector, and
System messages:

| Requirement | ASTM message | Field | Notes |
|:---|:---|:---|:---|
| Serial number, ANSI/CTA-2063-A | Basic ID | `IDType = Serial Number`, `UASID` | No session-ID option for broadcast modules — serial number only |
| UA latitude/longitude | Location/Vector | `Latitude`, `Longitude` | |
| UA geometric altitude | Location/Vector | `AltitudeGeo` | |
| Velocity | Location/Vector | `SpeedHorizontal` (and, for wire-format completeness, `SpeedVertical`, `Direction`) | §89.315(d) says "velocity" without further specifying horizontal/vertical/track breakdown |
| Take-off location latitude/longitude/altitude | System | `OperatorLatitude`, `OperatorLongitude`, `OperatorAltitudeGeo` with `OperatorLocationType = Takeoff` | There is **no live control-station/pilot-position field** for broadcast modules — take-off location is the only location-of-operator proxy defined |
| UTC time mark of the position source | Location/Vector | `TimeStamp` | |

([14 CFR §89.315](https://www.ecfr.gov/current/title-14/section-89.315),
[§89.320(a)](https://www.ecfr.gov/current/title-14/section-89.320))

### Performance floor

| Parameter | Requirement |
|:---|:---|
| Message rate | ≥ 1 message/second |
| Latency | ≤ 1.0 s from measurement to broadcast |
| UA position accuracy | ≤ 100 ft (95%) |
| UA altitude accuracy | ≤ 150 ft (95%) |
| Take-off location position/altitude accuracy | Same as above |

([14 CFR §89.320(h)](https://www.ecfr.gov/current/title-14/section-89.320))

## Transports

§89.320(g) is transport-agnostic: it requires only a non-proprietary, documented
broadcast specification on license-free ISM-band spectrum ([47 CFR Part
15](https://www.ecfr.gov/current/title-47/part-15)), designed to maximize receivable
range. No specific PHY is named, and **there is no requirement to run more than one
transport simultaneously**.

The FAA-accepted Means of Compliance, ASTM F3586-22, is an overlay of **ASTM
F3411-22a** (not the earlier F3411-19 edition) — this is the specific edition the US
accepted MOC is pinned to. F3411-22a's broadcast annexes cover Bluetooth 4 legacy
advertising, Bluetooth 5 Long Range, Wi-Fi Beacon vendor-IE, and Wi-Fi NAN — all four
of DRIFT's transports are within the accepted MOC's scope; none is individually
mandatory.

{: .note }
> The full ASTM F3411-22a/F3586-22 clause text is paywalled; the above is
> verified against public scope/abstract pages plus [ASTM F3586-22's Notice of
> Availability](https://www.federalregister.gov/documents/2022/08/11/2022-16997/
> accepted-means-of-compliance-remote-identification-of-unmanned).
> Whether any single transport alone would satisfy the "maximize range" language on its
> own has not been independently confirmed.

## Identity & registration fields

| You provide | Goes on the wire as | Format | Where to get it |
|:---|:---|:---|:---|
| Device serial number | Basic ID message, `UASID` | ANSI/CTA-2063-A-2019 | Assigned by the module's producer/manufacturer — not issued by the FAA |

No operator-registration-number broadcast field exists in the US rule — the FAA's
drone registration number is not a Part 89 broadcast requirement. A session ID
(privacy-preserving rotating identifier) is permitted for Standard RID aircraft but
**not** for broadcast modules — only the fixed serial number.

## Not required

- **Emergency status** — required for Standard RID aircraft ([§89.305(h)](https://www.ecfr.gov/current/title-14/section-89.305)) but absent from the broadcast-module element list entirely.
- **UA type / category-class marking** — no US analog to EU-style class marking; `UAType` is an ASTM wire-format artifact with no substantive CFR requirement behind it.
- **Dual transport (Bluetooth + Wi-Fi simultaneously)** — not required; any MOC-recognized transport, or several, satisfies the rule.

## Sources

- [14 CFR Part 89](https://www.ecfr.gov/current/title-14/part-89) (eCFR)
- [FAA Notice of Availability — Accepted Means of Compliance, Remote Identification of Unmanned Aircraft, 87 FR 49520](https://www.federalregister.gov/documents/2022/08/11/2022-16997/accepted-means-of-compliance-remote-identification-of-unmanned)
- [ASTM F3586-22 scope/significance & use](https://www.astm.org/f3586-22.html)
- [ASTM F3411-22a scope](https://www.astm.org/f3411-22a.html)
- [FAA — Remote ID overview](https://www.faa.gov/uas/getting_started/remote_id)
- Enforcement discretion notices: [87 FR 55685](https://www.federalregister.gov/documents/2022/09/12/2022-19644/enforcement-policy-regarding-production-requirements-for-standard-remote-identification-unmanned), [88 FR 63518](https://www.federalregister.gov/documents/2023/09/15/2023-20074/enforcement-policy-regarding-operator-compliance-deadline-for-remote-identification-of-unmanned)

Last verified 2026-09-10.

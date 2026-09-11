---
layout: page
title: Japan
parent: Regulations
nav_order: 4
permalink: /regulations/japan/
---
# Japan

Japan's Ministry of Land, Infrastructure, Transport and Tourism (MLIT), through its
Civil Aviation Bureau (JCAB), publishes a dedicated technical requirement document —
**"Requirements for remote ID devices and applications"** — citing [Civil
Aeronautics Act (航空法) Article 132-5(1)](https://www.mlit.go.jp/koku/content/001582250.pdf)
and Civil Aeronautics Act Enforcement Regulations Article 236-6(1)(ii). Unusually for
a non-English-speaking jurisdiction's technical standard, MLIT maintains and
publishes a complete English reference translation (the Japanese original governs
in case of conflict).

Japan's requirements are a materially larger extension of the base ASTM F3411
message set than the US/EU/UK profiles — this page focuses on where it diverges.

## Applicability

- Registration is mandatory for all drones/model aircraft **≥ 100 g** flying
  outdoors — a lower threshold than the US/EU's ~250 g registration triggers.
- The Remote ID equipment requirement supports both "unmanned aircraft with built-in
  remote ID **or external Remote ID device**" — the same add-on/retrofit concept as
  the US and EU/UK.
- Exemptions from the equipment obligation (registration is still required):
  aircraft registered during the 2021-12-20 to 2022-06-19 pre-registration window
  (permanent exemption); flights over a controlled/observed area with assigned
  monitors; tethered/moored flight with a sufficiently strong line.

## Timeline

| Date | Event |
|:---|:---|
| 2021-12-01 | Requirements document enacted |
| 2022-06-20 | UA registration mandate and original Remote ID requirement take effect |
| 2022-11-28 | Requirements document amended |
| 2022-12-05 | Amendment takes effect |

## Required messages & fields

| Requirement | ASTM message | Field | Notes |
|:---|:---|:---|:---|
| Government-issued registration ID | Basic ID | `IDType = CAA Registration ID` (ODID value `2`), `UASID = "JA." + JU…` | Broadcast **alongside**, not instead of, the serial number — this is the mandatory dual-identity requirement |
| Manufacturer serial number | Basic ID (second entry) | `IDType = Serial Number` (ODID value `1`), `UASID` | ANSI/CTA-2063-A |
| Authentication | Authentication | `AuthType = Message Set Signature` (ODID value `3`) | **Mandatory on every broadcast** — see below |
| Location/vector, System, etc. | (as ASTM F3411 baseline) | | |

The ASTM/ODID wire format's `IDType`/`AuthType` enum values line up exactly with
Japan's own numbering (`2` for the registration ID, `1` for the serial number, `3`
for the authentication type) — the same enum the vendored `libopendroneid` already
defines.

## Mandatory authentication — not Ed25519

Japan mandates the Authentication Message on every broadcast, but the algorithm is
**AES-128-CCM** (a symmetric MAC), not an asymmetric/public-key scheme:

- `AuthType = 3` ("Message Set Signature"), computed over the message set using a
  **16-byte shared key** provisioned **per-device by MLIT** via a Bluetooth 4.x GATT
  link (the "JCAB App" or a manufacturer app) — a separate provisioning channel from
  the Remote ID broadcast itself.
- A separate ECDSA P-256/SHA-256 signature also appears in the spec, but only to
  authenticate the **provisioning command data** written from the MLIT server into
  the device — it has nothing to do with the over-the-air broadcast.
- No reference to Ed25519/Curve25519/EdDSA appears anywhere in the specification.

## Message rate

A blanket **≥ 1 Hz** for the entire mandatory set (dual Basic ID + Location/Vector +
Authentication), sent as one Message Pack, with dynamic information transmitted
within 1 second of acquisition.

## Transports

| Allowed | Not allowed for the Remote ID broadcast |
|:---|:---|
| Bluetooth 5.x Long Range | Bluetooth 4 legacy advertising — explicitly excluded, including the ASTM-recommended `ADV_NONCONN_IND` backwards-compatibility fallback, which the spec says "shall not be applied" |
| Wi-Fi NAN (Wi-Fi Aware) | |
| Wi-Fi Beacon | |

At least one of the three allowed transports must carry the Message Pack. Bluetooth
4.x is still used elsewhere in the spec, but only for the separate device
provisioning/GATT link — not the RID broadcast.

EIRP floors: **+5 dBm** minimum for Bluetooth 5.x, **+11 dBm** minimum for Wi-Fi
Aware/Beacon (capped by Japan's Radio Act limits); target range ≥ 300 m under ideal
conditions.

## Identity & registration fields

| You provide | Goes on the wire as | Format | Where to get it |
|:---|:---|:---|:---|
| Registration ID | Basic ID message (2nd entry), `UASID` | `JA.` + government-issued `JU…` ID | Japan's drone registration system (DIPS) |
| Serial number | Basic ID message (1st entry), `UASID` | ANSI/CTA-2063-A | Assigned by the manufacturer |
| Authentication key | Authentication message | 16-byte AES-128-CCM key | Provisioned per-device by MLIT/JCAB after manufacturer enrollment — not user-supplied |

## Product/manufacturer requirements beyond the wire format

Unlike the US/EU/UK, a device cannot simply implement the correct bytes and be done:

1. **Japan Radio Act ("Giteki") technical-standard conformity certification** is
   required for the RF hardware/firmware combination.
2. **Manufacturer self-declaration and JCAB notification** must be filed before
   sale; JCAB then publishes a list of notified-compliant models.
3. **Per-manufacturer enrollment with MLIT/JCAB** is required to obtain the
   AES-128-CCM key material, provisioned into each unit over the Bluetooth 4.x GATT
   interface the specification defines.

There is no "just flash it and go" path for Japan independent of this
manufacturer-level enrollment process.

## Not required

- **Ed25519 or any asymmetric signature scheme** for the broadcast itself — a common
  misconception; see above.
- **Bluetooth 4 legacy advertising** as a satisfying transport on its own.

## Sources

- [MLIT — "Requirements for remote ID devices and applications" (English reference translation)](https://www.mlit.go.jp/koku/content/001582250.pdf)
- [MLIT — Japanese original (authoritative in case of conflict)](https://www.mlit.go.jp/koku/content/001444589.pdf)
- [MLIT — English drone registration portal](https://www.mlit.go.jp/koku/drone/en/index.html)

Last verified 2026-09-10.

---
layout: page
title: Broadcasting
permalink: /broadcasting/
nav_order: 2
---
# Broadcasting

DRIFT broadcasts Drone Remote ID on four radio transports at the same time, all built
from the same live MAVLink telemetry. The broadcast content (aircraft identification,
position, system status, operator) is identical on every transport — only the on-air
format and cadence differ.

## Transports

| Transport | Method | Cadence | Default |
|:---|:---|:---|:---|
| [Bluetooth 4](#bluetooth-4-legacy-advertising) | ASTM F3411 BLE advertising, one message per advertisement | 10 advertisements/s | always on |
| [Bluetooth 5 Long Range](#bluetooth-5-long-range) | ASTM F3411 BLE advertising, bundled Message Pack on Coded PHY | 1 pack/s | on |
| [Wi-Fi Beacon](#wi-fi-beacon) | ASTM F3411 Beacon vendor Information Element | refreshed 5x/s | on |
| [Wi-Fi NAN](#wifi-nan) | ASTM F3411 NAN action frame | ~2 frames/s | off |

### Bluetooth 4 (legacy advertising)
The baseline transport every Remote ID receiver understands: the individual messages
(Basic ID, Location, System, Self ID, Operator ID) are advertised one per slot on a
100 ms round-robin schedule — the minimum advertising interval permitted by the
Bluetooth Core Spec. Within each second the schedule allocates the message rates
prescribed by ASTM F3411: Location four times, Basic ID and System twice each,
Self ID and Operator ID once each. This transport cannot be turned off.

### Bluetooth 5 Long Range
The same data bundled into one Message Pack, advertised once per second on the
Bluetooth 5 Coded PHY (Long Range), which extends reception range roughly fourfold
compared to legacy advertising. On by default; can be switched off in the `Config`
tab of the dashboard.

### Wi-Fi Beacon
The Message Pack in a vendor Information Element on the Wi-Fi access point the module
itself operates — attached to both its beacons and its probe responses, refreshed
5 times per second. In practice this is the only Wi-Fi transport phones actually
decode. On by default; can be switched off in the `Config` tab of the dashboard.

### Wi-Fi NAN
The Message Pack in a Wi-Fi NAN (Neighbor Awareness Networking) action frame,
preceded by a NAN sync beacon, once per 512 ms discovery window. Only a handful of
Android handsets expose NAN reception and no regional profile requires it, so this
transport ships **off** by default — opt in via the `Config` tab of the dashboard.


## Receiver compatibility

| Receiver | Transports received |
|:---|:---|
| iPhone / iPad | Bluetooth 4 |
| Android (recent) | Bluetooth 4, Bluetooth 5 Long Range, Wi-Fi Beacon |
| Android (NAN capable, rare) | all four |

{: .note }
Apple devices do not receive any of the Wi-Fi Remote ID methods. Android reception of
Bluetooth 5 Long Range depends on the phone's chipset.


## Channel 6

The module's Wi-Fi access point is pinned to channel 6 — the channel the Remote ID
Wi-Fi transports (Beacon and NAN) are expected on. A phone connected to the module's
WiFi is therefore automatically on the right channel to receive them.


## Regulatory context

The transport set exists to cover the regional Remote ID frameworks that reference
the ASTM F3411 message set:

- **United States** — the FAA's Means of Compliance (F3586) accepts Bluetooth 4 +
  Bluetooth 5 Long Range, or Wi-Fi Beacon as the alternative. The default set covers it.
- **European Union** — prEN 4709-002 (Direct Remote ID) accepts any one of the four
  transports. The default set covers it.

Wi-Fi NAN is not required by any regional profile, which is why it is off by default.

{: .important }
> DRIFT makes no claims as to the compliance within your jurisdiction.
> Read the [Legal](/legal/) section before use!
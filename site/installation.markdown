---
layout: page
title: Installation
permalink: /installation/
---
# Installation

Installing DRIFT has a few steps that need to be completed.

1. [Wiring the module so it can receive MAVLink from the Flight Controller.](#hardware)
2. [Configure the Flight Controller to provide MAVLink telemetry to the module.](#flight-controller)
3. [Flashing the module firmware.](#firmware)
4. [Configure the module to your needs.](#configure)
5. [Test the module for correct operation.](#testing)



## Hardware

To make DRIFT work the only connection needed is one UART through which the Flight Controller 
sends [MAVLink](https://mavlink.io/) messages. These are used to determine the position and state of the drone to 
broadcast.

{: .note }
At the moment we only support ESP32-C3 Mini modules in our releases.
If you are familiar with PlatformIO you can modify the configuration to build for other modules and test.
Pull Requests are always welcome.


### ESP32-C3 Mini
We assume you are using a development module which has all the pins needed for booting the module, wired 
accordingly.

If you want to use a bare-bones module directly you have to populate a few more pins to make 
it work. For more details you can consult the [datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3-mini-1_datasheet_en.pdf) section *Peripheral Schematics*.

You can choose to flash via another interface, by default we assume you are using the built-in USB 
interface. This leaves the first serial line to be used for ingesting MAVLink messages from the flight controller.

#### Pinout

| Pin | Name | Function                                  |
|:----|:-----|:------------------------------------------|
| 26  | IO18 | USB_D- for Programming and Debug          |
| 27  | IO19 | USB_D+ for Programming and Debug          |
| 30  | RXD0 | MAVLink input Serial0                     |
| 31  | TXD0 | MAVLink output Serial0                    |

![ESP32-C3 Mini module pinout](/assets/images/pinout_esp32-c3-mini.png)



## Flight Controller

Once you have connected `Serial0` to a UART available on your Flight Controller, you can start with your 
software configuration.


### Betaflight

#### Hard- VS Soft-Serial
DRIFT can consume MAVLink messages from Betaflight both on hard- as well as soft-serial 
ports. If you have a hard-serial available, this is preferable for reliability.

If you would like to use a soft-serial port please consult the [Betaflight documentation](https://betaflight.com/docs/wiki/guides/current/softserial) on how to set it up before proceeding.

#### MAVLink Firmware Support
Depending on your current firmware version and build you might not have MAVLink support included.
With Cloud Build in Betaflight this is a very easy fix. Use the *Firmware Flasher* in your 
Betaflight Configurator as you normally would, flashing a new firmware and simply add the *Custom Define* `TELEMETRY_MAVLINK`.

![Betaflight Configurator - Firmware Flasher tab](/assets/images/betaflight_mavlink.png)

#### MAVLink Config
To make MAVLink work you only have to configure the relevant UART in your *Ports* tab to send 
MAVLink telemetry at `9600 baud`.

![Betaflight Configurator - Ports tab](/assets/images/betaflight_ports.png)



## Firmware

Firmware files are provided as part of each [release](https://github.com/wnagele/drift/releases) as `Assets`.

### Serial
For serial upload you can use `esptool` either locally or through your browser with web serial support.

#### Web
1. Open [https://espressif.github.io/esptool-js/](https://espressif.github.io/esptool-js/) in your browser.
2. In the `Program` section set the baud rate to `460800`.
3. Press `Connect` and choose the serial port your module is connected at.
4. Set the `Flash Address` to `0x0` and choose the firmware file ending in `-full.bin`.
5. Press `Program` and let the process finish.
6. Reset your board.

#### Local
```
esptool.py write_flash 0x0 firmware-esp32_c3_devkit-RELEASE-full.bin
```
Once finished reset your board.


### OTA

{: .important }
Over-the-Air (OTA) updates are only available after you have installed DRIFT on a module once.

To perform such an update simply connect to the WiFi AP of your module (default SSID is `DRI_<chipid>`).
Once connected open the web page at [http://drift.local/update](http://drift.local/update).

Upload the firmware file ending in `-ota.bin` for your module and wait for the process to finish.
It should automatically reset and boot into the updated firmware.



## Configure

1. Connect to your module via WiFi (default SSID is `DRI_<chipid>`).
2. Open a browser and go to the dashboard at [http://drift.local](http://drift.local).
3. All options for configuring the module are found in the `Config` tab



## Testing

1. Connect to your module via WiFi (default SSID is `DRI_<chipid>`).
2. Open a browser and go to the dashboard at [http://drift.local](http://drift.local).
3. The `Status` tab of the dashboard shows you if all necessary data is properly received.  
   For the GNSS status make sure your drone has a proper 3D position fix in your Flight Controller.
4. To verify the DRI broadcasts we recommend the free *Drone Scanner* app ([Android](https://play.google.com/store/apps/details?id=cz.dronetag.dronescanner), [iOS](https://apps.apple.com/gb/app/drone-scanner/id1644548782)).
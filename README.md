# Agrosmart firmware

Firmware for **Agrosmart**, the ESP32 controller of [Agromate](https://agromate-devs.github.io/website/), the open
source smart greenhouse. Agrosmart reads temperature, air humidity, soil moisture and the water tank level, and
autonomously drives the water pump, the humidifier and the LED grow light following the plant configuration set from the
[mobile app](https://github.com/agromate-devs/new_mobile_app).

It is built with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/) and talks to the
[Agromate server](https://github.com/agromate-devs/server) over MQTT.

📖 **Documentation:** <https://agromate-devs.github.io/website/docs>

## Features

- Pairing from the app over Wi-Fi with **ESP-Touch** (SmartConfig): the phone sends the Wi-Fi credentials and the MQTT
  broker address.
- Irrigation based on a **soil moisture threshold** or on a **fixed schedule**. The water quantity is converted to pump
  run time from the pump flow rate, and the pump doesn't run when the tank is empty.
- Humidification based on a **humidity threshold** or on a **fixed schedule**.
- **LED strip** grow light with configurable intensity, color and daily on/off window (time synced over NTP).
- Periodic metrics published on MQTT (`agrosmart/metrics`), configuration and commands received on
  `agrosmart/<MAC>/settings`.
- **OTA updates**: the device compares the SHA-256 of the published firmware with its own and updates itself.
- Optional RGB status LED.

## Supported hardware

| Target       | Status                                             |
| ------------ | -------------------------------------------------- |
| **ESP32-C5** | Official target: prebuilt firmware and web flasher |
| ESP32-S3     | Supported, build from source                       |
| ESP32        | Supported, build from source                       |

Supported peripherals: DHT11 / AM2301 / Si7021 temperature and humidity sensor, capacitive or resistive soil moisture
probe, water level sensor, relay-driven pump and humidifier, WS2812 / WS2811 / WS2816 / SK6812 LED strip.

Wiring and default pins for each chip:
[Building an Agrosmart](https://agromate-devs.github.io/website/docs?article=building_agrosmart).

## Installing

The easiest way is the **[web flasher](https://agromate-devs.github.io/website/flasher)**: connect an ESP32-C5 over USB
and flash it from the browser (Chrome or Edge).

Alternatively, download the images from the [latest release](https://github.com/agromate-devs/agrosmart/releases/latest)
and flash them with `esptool` as described in the release notes.

Then pair the device from the app: see
[Pairing the Agrosmart](https://agromate-devs.github.io/website/docs?article=pairing).

## Building from source

Requires [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html) **v6.1**, the version
used by the CI and the releases.

```bash
git clone https://github.com/agromate-devs/agrosmart.git
cd agrosmart
idf.py set-target esp32s3   # only if you don't use an ESP32-C5
idf.py menuconfig           # pins and options under "Agrosmart configuration"
idf.py build
idf.py -p PORT flash monitor
```

All the options (GPIOs, sensor types, intervals, pump flow rate, OTA, NTP...) are documented in
[Customizing the firmware](https://agromate-devs.github.io/website/docs?article=custom_firmware).

> [!TIP]
> `idf.py set-target` regenerates `sdkconfig`. Run `idf.py save-defconfig` to keep your options in `sdkconfig.defaults`
> before changing chip.

## Project structure

```text
main/
├── main.c                 # startup: NVS, Wi-Fi, pairing, MQTT, tasks
├── Kconfig.projbuild      # "Agrosmart configuration" menuconfig entries
└── idf_component.yml      # dependencies from the ESP Component Registry
components/
├── wifi/                  # Wi-Fi connection and SmartConfig pairing
├── mqtt_helper/           # broker connection and topic handling
├── plant_manager/         # plant config parser, sensors, monitoring tasks
├── water_manager/         # water pump
├── water_level_sensor/    # tank level
├── humidifier_manager/    # humidifier
├── light_manager/         # LED strip and light schedule
├── adc/                   # ADC helpers for the soil probe
├── keystore/              # NVS read/write helpers
├── ota_manager/           # over-the-air updates
└── telemetry/             # device info and diagnostic messages
tools/package_release.py   # packages the release images and the web flasher manifest
partitions.csv             # partition table: NVS, two OTA slots, core dump
sdkconfig.defaults         # configuration of the official release
```

## CI and releases

- **ESP-IDF build** builds the firmware for the ESP32-C5 on every push and pull request.
- **Release standard firmware** runs when a `v*` tag is pushed (`git tag v1.2.3 && git push origin v1.2.3`): it builds
  the firmware from `sdkconfig.defaults` and publishes the images, `manifest.json` (read by the web flasher) and
  `SHA256SUMS` as a GitHub release.

## Contributing

Bug reports, ideas and pull requests are welcome. Read
[Contributing to the code](https://agromate-devs.github.io/website/docs?article=contributing_code) for the repository
layout, the code style (`.clang-format`, Doxygen comments on public functions) and what to check before opening a pull
request.

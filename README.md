# TriviaPOD ESPHome prototype

An unofficial prototype to evaluate the **M5Stack TriviaPOD TP-01-A** with Home Assistant. It is not a production-ready port or an official M5Stack firmware.

The pin map comes from the [M5Stack StopWatch](https://docs.m5stack.com/en/core/StopWatch). M5Stack has not published a TriviaPOD schematic in its [product documentation](https://docs.m5stack.com/en/core/TriviaPOD), checked 2026-11-12. Tested hardware: ESP32-S3R8, 32 MB flash, 8 MB PSRAM, 466x466 round AMOLED.

## Install

**The generic demo is awaiting its on-device Wi-Fi/HA test.**

1. Connect TriviaPOD with a USB data cable and open the [USB installer](https://menemy.github.io/triviapod-esphome-demo/) in desktop Chrome or Edge.
2. Choose the TriviaPOD port and install the demo. Custom firmware replaces the original software; M5Stack warns that the official TriviaPOD experience may not be recoverable.
3. Enter Wi-Fi through the installer's [Improv Serial](https://esphome.io/components/improv_serial/) setup.
4. In HA, open **Settings > Devices & services** and add the discovered **TriviaPOD Prototype**. For manual setup, use its router address or `triviapod-demo-<MAC suffix>.local`, port 6053.

The demo embeds no Wi-Fi credentials or shared API key. Its local ESPHome API is **unencrypted**, for a trusted test LAN. For longer use, compile with a unique key using [the example](examples/encrypted-api.yaml). There is no OTA endpoint or Assist pipeline.

## Controls

Checks is the default page. Version/build time appear there, at boot, and in HA diagnostics.

| Control | Action |
| --- | --- |
| Yellow / Mic replay | Record about 3 seconds into volatile RAM, then play it locally. |
| Tone 1/2k / blue hold, then release | 1 kHz followed by 2 kHz, half a second each. |
| Blue short tap | Switch Checks / Camera. |
| Power short tap | Show Checks, pulse vibration, toggle LED. |
| Motor / LED | Local and HA test controls. |
| Dim / Bright | Adjust screen brightness. |
| Touch / drag | Show coordinates and publish input state. |

HA also exposes button counters, battery/USB/charging, IMU, read-only RTC and a WAV speaker. To test HA sound, upload [this synthetic WAV](docs/firmware/tone-1k-2k-16k-mono.wav) in **Media > My media > Manage > Add media**, then play it to **TriviaPOD Speaker** as an announcement. Audio is half-duplex; recordings stay in RAM. This is not doorbell two-way talk.

## Camera

Set **Camera Snapshot URL** in HA to a small, unauthenticated 4:3 JPEG URL on your LAN, preferably 352x264. Leave it empty to disable fetching. Do not put credentials/tokens in the URL.

The frame is center-cropped to fill the round display. The measured rate on one unit was about **1.06 fresh frames/s**, not video. Camera work pauses during audio and motor shutdown. For ringing, connect your sensor to **Doorbell Ring Test** with [an HA automation](examples/doorbell-automation.yaml).

## Checked on one unit

| Function | Result |
| --- | --- |
| Display / circular camera | Colors and full-circle appearance checked. |
| Touch / three buttons | Press, release and drag events checked. Calibration not measured. |
| Mic / speaker / 1-2 kHz tone | Intelligible replay and audible tone checked. |
| Vibration | Felt and start/stop readbacks checked. |
| BMI270 / battery / USB | Moving IMU and voltage readings checked; not calibrated. |
| Charging / brightness / LED | Software commands/readbacks checked; physical checks incomplete. |
| RTC | Responds, but calendar invalid. Read-only; no time/backup writes. |
| Generic Wi-Fi / HA | Improv, adoption, entities, WAV and ring test pending on this image. |

Hardware checks cover one TP-01-A. Other revisions may differ. Include the visible version and relevant logs in bug reports, with credentials/URLs removed.

## Build

Linux/macOS or WSL2, with **ESPHome 2026.9.1**:

```sh
uv tool install esphome==2026.9.1
python3 tools/build.py --stage-web
python3 tools/check.py
```

Each build gets a unique version, merged ESP32-S3/32 MB image, source hashes and checksums in `docs/firmware/`. The installer uses a hash-specific image URL. CLI alternative:

```sh
esptool --chip esp32s3 --port YOUR_PORT write-flash --flash-size 32MB 0x0 triviapod-demo.factory.bin
```

Source/configuration is Apache-2.0; dependencies retain their licenses. See [NOTICE.md](NOTICE.md), [font license](fonts/OFL.txt) and [build metadata](docs/firmware/build.json).

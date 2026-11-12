# TriviaPOD ESPHome prototype

**An experimental prototype to check whether the M5Stack TriviaPOD is suitable for Home Assistant.** This is an evaluation demo, not a production-ready port or an official M5Stack firmware.

По-русски: это прототип - просто проверить, подходит ли TriviaPOD для Home Assistant.

Target: **M5Stack TriviaPOD TP-01-A**, ESP32-S3R8, 32 MB flash, 8 MB octal PSRAM, round 466×466 AMOLED. Do not install it on a different ESP32 device merely because the chip is an ESP32-S3.

## Try the demo

The intended installation path is the [browser USB installer](https://menemy.github.io/triviapod-esphome-demo/) once this repository and its Pages site are published. The local preview is `docs/index.html`; it does not publish anything.

1. Connect TriviaPOD with a USB data cable. Open the installer in a browser with Web Serial support, such as Chrome or Edge on a desktop computer.
2. Select **Install prototype**, select the TriviaPOD USB device, and follow the prompts. This replaces the current firmware. Keep your own backup/configuration if you need to return to it.
3. After installation, use the installer's **Wi-Fi** setup. This uses [Improv via Serial](https://esphome.io/components/improv_serial/); no shared Wi-Fi credentials are compiled into the download.
4. In Home Assistant, open **Settings → Devices & services** and add the discovered **TriviaPOD Prototype** ESPHome device. If discovery does not appear, use its `triviapod-demo-<MAC suffix>.local` hostname or the address shown by your router, port 6053.

**Prototype limitation:** the generic demo uses an **unencrypted local ESPHome API**, so it needs no shared precompiled API key. Use it only on your trusted test LAN. For a longer-lived installation, compile your own configuration with a unique API encryption key; see [the example](examples/encrypted-api.yaml). No OTA update endpoint or Assist/voice-assistant pipeline is included.

The exported generic firmware/build has not yet been flashed and provisioned on hardware. The hardware implementation below was exercised on one TP-01-A during bring-up; that does not establish compatibility with every revision or validate the browser provisioning flow. The installer and build metadata state this explicitly.

## On-device checks

The **Checks** page is the default. The exact firmware/build identity is shown on this page and for five seconds at boot, and is also available in HA as **Firmware Version** / **Firmware Build UTC**.

| Control | What it does |
| --- | --- |
| Yellow button / Mic replay | Record about 3 seconds into volatile RAM, then replay locally. Nothing is saved to disk or sent to a speech service. |
| Tone 1/2k / hold blue about 1 second, then release | One beep: 1 kHz then 2 kHz, about half a second each. The quiet 440 Hz diagnostic was removed from the user interface. |
| Blue short tap | Switch Checks / Camera pages. |
| Power short tap | Show Checks, pulse vibration, toggle the PMIC LED. Keep presses short; PMIC emergency power behavior is not replaced. |
| Motor / LED | Local test controls, also available from HA. |
| Dim / Bright | Adjust the display brightness. |
| Touch and drag | Show touch coordinates / marker and publish input state to HA. |

The HA device also exposes button states/counters, battery and USB voltage/detection, charge state, IMU values, read-only RTC status, display brightness, guarded WAV media playback, and local test buttons.

To check sound **from HA**, download the [synthetic test WAV](docs/firmware/tone-1k-2k-16k-mono.wav), upload it under HA **Media → My media → Manage → Add media**, and play it to **TriviaPOD Speaker** as an announcement. It is a 1-second, 16 kHz, mono, 16-bit 1/2 kHz beep generated from the same tone code, not a microphone recording. This does not require Assist. [HA media upload instructions](https://www.home-assistant.io/more-info/local-media/add-media/).

The microphone and speaker share a half-duplex I2S bus. Local replay and HA playback are serialized; a conflicting playback request is rejected. This is not two-way talk to a doorbell, not an Assist satellite, and does not require STT/TTS.

## Optional camera page

In HA, set the **Camera Snapshot URL** configuration entity to a small, unauthenticated JPEG endpoint on your LAN. No household URL is compiled into the demo. Do not put access tokens or credentials in the URL. An empty URL disables fetching and shows a setup prompt.

The tested source format is a small 4:3 JPEG, 352×264. The firmware keeps a last-good RGB565 frame, center-crops it to a square, and scales it uniformly to fill the whole circular display. Buttons/text are overlays. For other source aspect ratios, prepare a 4:3 snapshot upstream; this prototype uses a fixed decode/cache size.

Example format only: `http://YOUR-FRIGATE-HOST:5000/api/YOUR-CAMERA/latest.jpg?height=264`.

This is JPEG polling, not video. On the tested private bring-up image, one 60.4 s sample showed 64 new frames/software draws, about **1.06 frames/s**. Actual rate depends on snapshot source, Wi-Fi, decoding and display load. The new generic export has not yet been measured. Camera work is paused during local audio, HA playback and motor shutdown.

To react to a real doorbell, create your own HA automation which presses this device's **Doorbell Ring Test** entity when your ring sensor rises. See [the automation example](examples/doorbell-automation.yaml); no particular household sensor is subscribed in the demo.

## What was checked

| Part | Bring-up evidence / open item |
| --- | --- |
| Display, full-circle picture | Owner saw correct display colors and later confirmed the full-circle camera looked normal. |
| Touch, yellow/blue/power inputs | Fresh press/release/drag event logs; physical actions exercised. Full-panel touch calibration was not measured. |
| Microphone, speaker/amp | Owner heard recognizable/intelligible local replay; final integration replay also accepted by ears. |
| Tone 1/2 kHz | Owner heard it working; original 440 Hz signal was audible but quiet. |
| Vibration | Felt by the owner, with motor start/stop readbacks. A nominal 200 ms pulse can be longer under scheduling load. |
| BMI270 | Correct ID plus changing acceleration/gyro/temperature samples during movement; not calibrated. |
| Battery / USB | PMIC readings and on-screen values checked. No unplug/replug transition or meter calibration test. |
| Charging | Charge-enable bit is set with bit-only read/modify/write/readback. Active-charge/current behavior not fully checked. |
| LED / brightness | Software writes/readbacks/brightness commands checked; explicit final physical confirmation is still incomplete. |
| RTC | Device responds at 0x32, but calendar fields were invalid. Read-only probe only; no time/backup-charge configuration writes. |
| HA | API/entities compile; end-to-end adoption, real ring automation, media playback and new Improv provisioning still need owner evaluation. |

These observations are from the bring-up code on one unit. The generic prebuilt export is an additional, not yet hardware-validated configuration. Report problems with your hardware revision, visible firmware version, and relevant diagnostic logs; remove network credentials/URLs before sharing logs.

## Build from source

The source helper targets Linux/macOS; on Windows use WSL2. The tested compiler is **ESPHome 2026.9.1**. Install it in an isolated environment using your normal package policy, for example `uv tool install esphome==2026.9.1`, then run:

```sh
python3 tools/build.py --stage-web
```

This reserves a unique version before each build, including failed attempts, compiles only the generic configuration, and writes build metadata, source hashes and image checksums. It stages the merged factory image and ESP Web Tools manifest in `docs/`; it does not open a serial port, flash, create a remote, push, or deploy Pages. New visible versions include the prototype release, build number and UTC date/time. Use the helper for deployable builds rather than reusing a stale generated identity.

`requirements.txt` pins the compiler. The font is vendored under its OFL license, so building does not depend on a macOS system font. The firmware image is merged for ESP32-S3 / 32 MB flash, offset 0.

For command-line installation, download the factory image and compare its SHA-256 with `docs/firmware/SHA256SUMS`, then use your own selected TriviaPOD port:

```sh
esptool --chip esp32s3 --port YOUR_PORT write-flash --flash-size 32MB 0x0 triviapod-demo.factory.bin
```

No serial port is chosen automatically by this repository. If your device reports security/flash-protection errors, stop rather than changing eFuses or encryption settings.

Host checks:

```sh
python3 tools/check.py
```

[Build metadata](docs/firmware/build.json) identifies the exact generic image. Historical bring-up images, private Wi-Fi/API files, serial captures and factory dumps are not part of this repository.

## Project layout

- `triviapod-demo.yaml`, `packages/`: portable demo / HA entities / UI.
- `components/`: local M5PM1/M5IOE1 board glue and media arbitration.
- `tools/`, `tests/`: build, export validation and focused host checks.
- `docs/`: static USB installer and generic firmware, ready for Pages after publication approval.
- `examples/`: optional encrypted API and user-owned doorbell automation.

Source/configuration code is Apache-2.0. ESPHome and firmware dependencies retain their own licenses. See [NOTICE.md](NOTICE.md) and [fonts/OFL.txt](fonts/OFL.txt).

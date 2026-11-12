# Notices

This is an independent experimental configuration, not an official M5Stack or Home Assistant product.

- Project source/configuration: Apache License 2.0, see LICENSE.
- Pin assignments and board sequencing were derived from the Apache-2.0 Muse Gadget SDK `board_m5stack_triviapod.c` bring-up and checked against the [M5Stack StopWatch documentation](https://docs.m5stack.com/en/core/StopWatch). Local board glue preserves its Apache-2.0 SPDX notices. No M5Stack external component is vendored.
- Firmware is built with [ESPHome 2026.9.1](https://github.com/esphome/esphome/tree/2026.9.1), which is GPL-3.0; ESP-IDF and other dependencies retain their own licenses. This repository supplies the configuration/custom component source and exact build metadata used for the generic image. Reproduce with the pinned ESPHome release; do not treat the root Apache license as relicensing firmware dependencies.
- Noto Sans is from the official [Google Fonts source](https://github.com/google/fonts/tree/main/ofl/notosans), licensed under SIL Open Font License 1.1. The complete license/copyright notice is in fonts/OFL.txt. Vendored font SHA-256: bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d.
- The browser installer loads [ESP Web Tools](https://esphome.github.io/esp-web-tools/) major 10 from its documented CDN. No script is given a serial port until the user chooses Connect/Install and grants browser permission.

No published TriviaPOD schematic/pin map was available to this project during bring-up; the [official product page](https://docs.m5stack.com/en/core/TriviaPOD) checked 2026-11-12 includes no schematic/pin-map section. The board mapping here is independent bring-up work, not an official M5Stack schematic-based port.

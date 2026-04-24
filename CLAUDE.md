# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

SA32 / SokolAudio: ESP-IDF firmware for an ESP32-S3 network + Bluetooth Hi-Fi amplifier. The single firmware project lives under [smart_amp_proto/](smart_amp_proto/).

## Toolchain requirements

- **ESP-IDF v5.2.x** (`idf.py --version` must print 5.2.x; the `idf_component.yml` pins `>=5.2,<6.0`)
- **ESP-ADF v2.6** checked out separately. `ADF_PATH` **must** be exported before any build — the root [smart_amp_proto/CMakeLists.txt](smart_amp_proto/CMakeLists.txt) calls `message(FATAL_ERROR ...)` if it is unset and includes `$ENV{ADF_PATH}/CMakeLists.txt` **before** the standard IDF project include. The project cannot be configured with plain IDF alone.
- Target is `esp32s3` exclusively (set in `sdkconfig.defaults`).

## Build / flash / monitor

All commands run from `smart_amp_proto/`:

```
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor       # Ctrl+] to exit monitor
idf.py menuconfig                    # project options live under "SokolAudio (Smart Amp) Configuration"
```

There is no host-side test suite, lint step, or CI configuration in this repo. Validation is done on-device via the serial CLI (`sokol>` prompt): `help`, `tone`, `volume`, `source <wifi|bt|none>`, `info`, `heap`, `reboot`, and `wifi set <ssid> <pass>` (requires reboot).

## Architecture

The firmware is a single IDF component ([smart_amp_proto/main/](smart_amp_proto/main/)) organized into layers. `app_main` in [main.c](smart_amp_proto/main/main.c) enforces a strict boot order that other code relies on — do not reorder it:

1. `nvs_cfg_init` — persisted config is read by later stages (volume, SSID).
2. `amp_control_init` — **amp comes up muted** (GPIO21 HIGH) before any audio pin toggles. The boot mute is dropped only at the end of `app_main`.
3. `oled_init` (I²C bus + splash), then `rotary_ec11_init`.
4. `i2s_pcm5102a_init` → `i2s_dma_init` → `volume_ctrl_init`. The DMA pump ([audio/i2s_dma.c](smart_amp_proto/main/audio/i2s_dma.c)) owns the single PSRAM ringbuffer that every source writes into.
5. `stream_decoder_init` (FLAC/Opus hooks; currently returns `ESP_ERR_NOT_SUPPORTED` until ADF pipeline is wired).
6. `wifi_sta_init` → `udp_receiver_init` → `bt_a2dp_sink_init`. WiFi and BT Classic coexist on the shared radio.
7. `source_manager_init` picks the boot source and calls the corresponding transport enabler.

### Audio path

```
UDP receiver ─┐
              ├─► source_manager ─► volume_ctrl ─► i2s_dma (ringbuf) ─► I²S → PCM5102A → TPA3110D2
BT A2DP sink ─┘
```

- Only one source is active at a time. `source_manager_cycle` / `source_manager_select` are the only legal way to switch; they flush the DMA ringbuffer and enable/disable the transport callbacks.
- Phase 1 format is fixed **44.1 kHz / 2ch / S16LE**. `i2s_dma_reconfigure` exists but is not exercised end-to-end.
- LDAC and aptX are intentionally **excluded** (proprietary licenses). Do not add them.

### UI

A single `ui_task` pinned to Core 0 consumes `rotary_ec11_queue()` events: CW/CCW adjusts volume in ±2% steps and persists to NVS; short press toggles amp mute; long press cycles the audio source. The OLED is redrawn every loop tick (500 ms timeout).

### Storage & partitions

[partitions.csv](smart_amp_proto/partitions.csv) defines a 16 MB layout with `factory` + `ota_0` + `ota_1` (2 MB each), an encrypted `nvs_keys`, and a 1 MB SPIFFS `storage`. OTA slots are reserved but no updater is wired yet.

## Hardware pinout (authoritative)

Defined in [smart_amp_proto/README.md](smart_amp_proto/README.md#2-wiring). Key invariants:
- I²S: BCLK=5, LRCK=6, DOUT=7, optional MCLK=15 (gated by `CONFIG_SOKOL_DAC_MCLK_ENABLE`).
- I²C: SDA=8, SCL=9 at 400 kHz (SSD1306 @ 0x3C).
- Encoder: A=10, B=11, SW=12 (active LOW, debounced in `rotary_ec11.c`).
- **GPIO 21 = amp MUTE, active HIGH** — any code path that touches audio must honor the mute-before-configure rule.
- Optional PCM5102A XSMT on GPIO13, gated by `CONFIG_SOKOL_DAC_MUTE_PIN_ENABLE`.

## UDP stream protocol

Port from `CONFIG_SOKOL_UDP_PORT` (default 5005). Packet header is 11 bytes LE: magic `'SOKA'`, codec byte (0 = raw PCM S16LE), 24-bit sample rate, 1-byte channel count, 2-byte payload length, followed by interleaved S16 samples. See [smart_amp_proto/README.md](smart_amp_proto/README.md#wifi-raw-pcm-over-udp) for a reference Python sender.

## Current scaffolding / pending work

Called out in [smart_amp_proto/README.md](smart_amp_proto/README.md#7-known-prototype-limitations-v2-todo):
- FLAC/Opus decode is stubbed — `stream_decoder` accepts codec IDs but returns `ESP_ERR_NOT_SUPPORTED` until the ADF `audio_pipeline` is hooked up in [codecs/flac_decoder.c](smart_amp_proto/main/codecs/flac_decoder.c).
- AVRCP callback exists but play/pause/next are not routed into `source_manager`.
- OTA updater, external I²S daughter-board mux (`SRC_I2S_EXT`), and multi-page OLED menu are TODO.
- `CONFIG_PM_ENABLE=n` is deliberate — light-sleep breaks I²S clocking.

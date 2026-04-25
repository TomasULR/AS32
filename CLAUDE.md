# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

SA32 / SokolAudio: ESP-IDF firmware for an ESP32-S3 network + Bluetooth Hi-Fi amplifier. The single firmware project lives under [smart_amp_proto/](smart_amp_proto/).

**Current revision: v1.1** — externí BT receiver (BK3266) + HW I²S MUX, IRM-45-24 PSU, HLK-5M05 standalone power, real ADF pipeline for FLAC/Opus, AVRCP routing, HTTPS OTA. Phase 1 used native ESP32 BT (does not work on S3) — that path is gone.

## Toolchain requirements

- **ESP-IDF v5.2.x** (`idf.py --version` must print 5.2.x; `idf_component.yml` pins `>=5.2,<6.0`)
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

No host-side test suite. Validation is on-device via the serial CLI (`sokol>` prompt). v1.1 commands: `help`, `tone`, `volume`, `source <wifi|bt|none>`, `info`, `heap`, `reboot`, `wifi set <ssid> <pass>`, `bt <play|pause|next|prev|reset|status>`, `ota <url>`, `ota mark-valid`.

## v1.1 architecture

The firmware is a single IDF component ([smart_amp_proto/main/](smart_amp_proto/main/)). `app_main` in [main.c](smart_amp_proto/main/main.c) enforces a strict boot order — do not reorder it:

1. `nvs_cfg_init` — persisted config read by later stages.
2. `amp_control_init` — **amp comes up muted** (GPIO 21 HIGH) before any audio pin toggles.
3. `oled_init` (I²C bus + splash), then `rotary_ec11_init`.
4. `i2s_mux_init(GPIO_NUM_4)` — **NEW v1.1.** Hardware MUX between ESP I²S and BK3266 I²S, default = ESP.
5. `i2s_pcm5102a_init` → `i2s_dma_init` → `volume_ctrl_init`. The DMA pump owns the single PSRAM ringbuffer.
6. `stream_decoder_init` — **NEW v1.1.** Real ADF audio_pipeline scaffolding for FLAC + Opus.
7. `wifi_sta_init` → `udp_receiver_init`.
8. `bt_a2dp_sink_init` — **NEW v1.1.** UART2 driver for external BK3266 BT receiver (replaces native A2DP). Spawns `bk3266_rx` task that parses `+STAT:*` notifications and dispatches AVRCP events.
9. `source_manager_init` — registers AVRCP callback with `bt_a2dp_sink_on_avrcp` and switches to last-used source.
10. `ui_task`, `serial_cli_start`, `heap_debug_start`, `ota_updater_init`, then OTA validate task (auto-marks boot valid after 30 s).
11. Drop boot mute.

### Audio path

```
WiFi UDP ─► stream_decoder (RAW or ADF FLAC/Opus) ─► volume_ctrl ─► i2s_dma ─► I²S ─┐
                                                                                     ├─► HW MUX ─► PCM5102A ─► TPA3110D2
BT (phone) ─► BK3266 chip ─► I²S out ──────────────────────────────────────────────┘
```

- **Source switching:** `source_manager_select(SRC_X)` mutes amp → flushes DMA → toggles `i2s_mux_select()` → enables/disables UDP receiver and BT controller → unmutes.
- **BT audio bypasses ESP32 entirely** — it flows BK3266 → MUX → DAC. Software volume control does not apply to BT path; volume is controlled at the phone (or via `bt` CLI command which sends `AT+VOL+/-`).
- **ADF pipeline shape** (`codecs/stream_decoder.c`): `raw_in (writer) → flac_decoder | opus_decoder → raw_out (reader) → pump task → i2s_dma`. Switching codecs at runtime tears down the previous pipeline cleanly.
- LDAC and aptX are intentionally **excluded** (proprietary licenses). Do not add them.
- `codecs/aac_sbc_decoder.c` is a **deprecated stub** — the v1.1 BT path doesn't decode SBC in firmware (BK3266 does it).

### UI behavior

`ui_task` on Core 0 consumes `rotary_ec11_queue()`:
- CW/CCW: ±2% volume, persisted to NVS. **In BT mode also sends `AT+VOL+/-` to BK3266.**
- Short press: amp mute toggle. **In BT mode also sends `AT+CC/CD` (play/pause).**
- Long press: cycle source (WIFI → BT → NONE).

### OTA

- `network/ota_updater.c` uses `esp_https_ota` with mozilla CA bundle (`MBEDTLS_CERTIFICATE_BUNDLE`).
- `BOOTLOADER_APP_ROLLBACK_ENABLE=y`: new image starts in PENDING_VERIFY state. Main spawns `ota_validate_task` which calls `esp_ota_mark_app_valid_cancel_rollback()` after 30 s of stable boot. Earlier panic ⇒ bootloader rolls back next boot.
- HTTPS always allowed. HTTP allowed only for RFC 1918 local addresses (192.168/10/172.16-31) — enforced in `url_is_http_local()`.
- AMP is muted during OTA download to avoid audio glitch.

### Storage & partitions

[partitions.csv](smart_amp_proto/partitions.csv): 16 MB layout with `factory` + `ota_0` + `ota_1` (2 MB each), encrypted `nvs_keys`, 1 MB SPIFFS `storage`.

## Hardware pinout v1.1 (authoritative)

| GPIO | Function | Notes |
|---|---|---|
| 5, 6, 7 | I²S BCLK / LRCK / DOUT | through MUX → PCM5102A |
| 15 | I²S MCLK (optional) | gated by `CONFIG_SOKOL_DAC_MCLK_ENABLE` |
| 8, 9 | I²C SDA / SCL | SSD1306 @ 0x3C, 400 kHz |
| 10, 11, 12 | EC11 A / B / SW | int. pull-up + IRQ |
| 21 | AMP MUTE | active HIGH = silent, default at boot |
| 13 | DAC XSMT (optional) | gated by `CONFIG_SOKOL_DAC_MUTE_PIN_ENABLE` |
| **4** | **I²S MUX SEL** | **v1.1 NEW.** 0 = ESP I²S, 1 = BK3266 I²S |
| **14** | **BK3266 RESET** | **v1.1 NEW.** Active LOW pulse on init |
| **17** | **UART2 TX → BK3266 RX** | **v1.1 NEW.** AT command stream, 115200 baud |
| **18** | **UART2 RX ← BK3266 TX** | **v1.1 NEW.** `+STAT:*` parser |
| 38, 39 | LED status (optional) | gated by `CONFIG_SOKOL_STATUS_LEDS_ENABLE` |

Don't use 26-32 (flash) or 35-37 (octal PSRAM on N16R8).

## UDP stream protocol

Port from `CONFIG_SOKOL_UDP_PORT` (default 5005). 11-byte LE header: magic `'SOKA'`, codec byte (0=PCM, 1=FLAC, 2=Opus), 24-bit sample rate, channel count, 2-byte payload length, then payload (S16 samples for PCM, raw codec frames for FLAC/Opus).

## Hardware design specifics worth knowing

- **TPA3110D2 GAIN must be set to 12 dB** (not default 20 dB) on the XH-A232 module. PCM5102A line-out is 2.1 Vrms but TPA3110 input clips above 1 Vrms at 20 dB gain. Documented in `docs/SA32_navod_sestaveni.pdf` section 12 troubleshooting.
- **PSU is IRM-45-24 (1.88 A)**, not the original IRM-20-24. The smaller unit caused restart on bass transients (1.44 A peak vs 0.83 A capability).
- **Standalone 5 V supply** comes from HLK-5M05 isolated buck (24 V → 5 V / 1 A). The DevKit's onboard AMS1117 (5→3.3 V) is at thermal margin (~0.93 W peak); offload DAC/OLED/BK3266/MUX to an external AMS1117 LDO instead.
- **Star ground** at PSU output. Audio cable shield from DAC → AMP grounded only at the DAC end (otherwise 50 Hz hum from ground loop).

## Documentation outputs (under [docs/](docs/))

- `SA32_navod_sestaveni.pdf` — 14-page návod (CZ): assembly, schematic, GPIO map, firmware bring-up, ADF pipeline, AVRCP, OTA, validation report (15 checks), troubleshooting.
- `SA32_BOM.xlsx` — 4 sheets: BOM v1.1 (with eshop hyperlinks), GPIO map, power budget, validation matrix.
- `build_pdf.py`, `build_bom.py` — generators. Re-run after any HW or pinout change.

Both PDF and XLSX are regenerated by running the Python scripts; require `reportlab` and `openpyxl` (`pip3 install --user reportlab openpyxl`).

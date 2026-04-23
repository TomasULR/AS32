# SokolAudio — Smart Hi-Fi Amp Prototype

Prototype firmware for an ESP32-S3-driven Hi-Fi amplifier. Streams audio over
WiFi (UDP) or Bluetooth (A2DP/SBC), drives a PCM5102A I²S DAC feeding a
TPA3110D2 class-D module, with an SSD1306 OLED and EC11 rotary for local
control.

Phase 1 (this drop) boots to a muted amp, brings up I²S + OLED + encoder,
accepts raw-PCM UDP packets and SBC over A2DP. FLAC/Opus decoding is
scaffolded for ADF pipeline integration (see `codecs/flac_decoder.c`). LDAC
and aptX are intentionally disabled — proprietary, not shipped.

## 1. Toolchain setup (Windows, PowerShell)

You need **ESP-IDF v5.2.x** and a compatible **ESP-ADF** checkout. The root
`CMakeLists.txt` refuses to configure if `ADF_PATH` is not set.

```powershell
# ESP-IDF: install once via the official Windows installer from
#   https://dl.espressif.com/dl/esp-idf/
# It sets up Python, Ninja, the toolchain, and an "ESP-IDF Powershell"
# shortcut. After install, launch that shortcut (it runs export.ps1).
idf.py --version    # should print v5.2.x

# ESP-ADF (clone outside the project)
git clone --recursive https://github.com/espressif/esp-adf.git $env:USERPROFILE\esp\esp-adf
git -C $env:USERPROFILE\esp\esp-adf checkout v2.6
$env:ADF_PATH = "$env:USERPROFILE\esp\esp-adf"
```

To make `ADF_PATH` permanent, add it via System Properties → Environment
Variables, or prepend it to the ESP-IDF PowerShell shortcut.

## 2. Wiring

```
ESP32-S3-DevKitC-1 (WROOM-1-N16R8)

   +---------------------+                     +----------+
   |             GPIO 5  |---- BCLK --------->| PCM5102A |---- L/R ---> TPA3110D2 ---> 8Ω speakers
   |             GPIO 6  |---- LRCK --------->|   DAC    |
   |             GPIO 7  |---- DOUT --------->|          |
   |             GPIO 15 |---- MCLK (opt.)-->|          |
   |                     |                     +----------+
   |             GPIO 8  |---- SDA ----------.       ^
   |             GPIO 9  |---- SCL --------. |       |  GPIO 21 -> MUTE (active HIGH = silent)
   |                     |                  \|       |
   |             GPIO 10 |---- ENC A  (EC11) oled    |
   |             GPIO 11 |---- ENC B         |       |
   |             GPIO 12 |---- ENC SW        |       |
   |             GPIO 21 |----------- AMP MUTE ------+
   |             GPIO 13 |---- DAC XSMT (optional; Kconfig)
   +---------------------+
```

### Pin table

| Function      | GPIO | Notes                                           |
| ------------- | ---- | ----------------------------------------------- |
| I²S BCLK      | 5    | to PCM5102A BCK                                 |
| I²S LRCK      | 6    | to PCM5102A LCK                                 |
| I²S DOUT      | 7    | to PCM5102A DIN                                 |
| I²S MCLK      | 15   | optional; enable via `menuconfig → SokolAudio`  |
| I²C SDA       | 8    | SSD1306 + any future I²C peripherals            |
| I²C SCL       | 9    | 400 kHz, internal pull-ups enabled              |
| Encoder A     | 10   | internal pull-up, IRQ any edge                  |
| Encoder B     | 11   | internal pull-up, IRQ any edge                  |
| Encoder SW    | 12   | active LOW, debounced in `rotary_ec11.c`        |
| Amp MUTE      | 21   | active HIGH mutes TPA3110D2                     |
| DAC XSMT      | 13   | optional; most breakouts omit this pin          |

Power: 24 V DC barrel jack → TPA3110D2 VIN. Derive 5 V (for OLED) from the
DAC module's LDO if available or from a Mean Well IRM-20-24. 3.3 V to the
ESP32-S3 DevKit is supplied via USB during development.

## 3. Build, flash, monitor

```powershell
cd c:\Users\tomas\Code\SokolAudio\smart_amp_proto
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor     # adjust COM port
```

Exit monitor with `Ctrl+]`.

First build pulls ESP-ADF components under `$ADF_PATH`. If you see
`ADF_PATH is not set`, re-run `$env:ADF_PATH = ...` in your shell.

## 4. Serial CLI

Open the monitor, press Enter — you should see `sokol>`.

| Command                     | Description                                   |
| --------------------------- | --------------------------------------------- |
| `help`                      | List all commands                             |
| `tone [hz] [ms] [amp%]`     | Play a sine tone (defaults 1000 Hz / 1 s / 30%) |
| `volume [0..100]`           | Show or set volume (persists to NVS)           |
| `source <wifi\|bt\|none>`   | Switch active audio source                    |
| `wifi set <ssid> <pass>`    | Save WiFi credentials, then `reboot`          |
| `info`                      | One-shot device status                        |
| `heap`                      | Log internal + PSRAM heap usage               |
| `reboot`                    | Software reset                                |

## 5. Streaming audio

### WiFi (raw PCM over UDP)

Port **5005**. Packet format (little-endian):

```
0..3   magic   'S' 'O' 'K' 'A'
4      codec   0 = raw PCM S16LE
5..7   rate    sample rate (24-bit)
8      ch      channels
9..10  len     payload length (uint16)
11..   payload interleaved S16 samples
```

A minimal sender in Python:

```python
import socket, struct, sys
with open(sys.argv[1], 'rb') as f: pcm = f.read()
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
mtu = 1024
rate, ch = 44100, 2
seq = 0
for i in range(0, len(pcm), mtu):
    chunk = pcm[i:i+mtu]
    hdr = b'SOKA' + bytes([0]) + rate.to_bytes(3,'little') + bytes([ch]) + len(chunk).to_bytes(2,'little')
    s.sendto(hdr + chunk, ('192.168.x.y', 5005))
```

### Bluetooth (A2DP/SBC)

Device advertises as **SokolAudio**. Pair from a phone or laptop and start
playback; the firmware decodes SBC, applies volume, and feeds I²S.

## 6. Verified CZ/EU parts (qty 1–2 pcs)

| Part                            | Suppliers (search terms)                                                      |
| ------------------------------- | ----------------------------------------------------------------------------- |
| ESP32-S3-DevKitC-1 (N16R8)      | PUHY.cz, Mouser.cz, DigiKey.cz, LaskaKit.cz                                    |
| PCM5102A I²S breakout           | LaskaKit.cz, TinyTronics.nl, AliExpress EU — "PCM5102A DAC module"            |
| TPA3110D2 2×15 W module         | AliExpress EU — "TPA3110D2 2x15W amplifier board", or GM Electronic listings   |
| SSD1306 128×64 OLED (I²C, 0x3C) | Robotstore.cz, Pajtech.cz, LaskaKit.cz                                         |
| Alps EC11 rotary encoder (24 pulses, tactile SW) | Mouser.cz, Farnell.cz, TME.eu                               |
| 24 V 20 W PSU (IRM-20-24)       | GM Electronic (gme.cz), Mouser.cz                                              |
| AMS1117-3.3 regulator (3.3 V rail) | GM Electronic, Mouser.cz                                                   |
| AP2112-1.8 regulator (optional analog rail) | Mouser.cz, Farnell.cz                                              |

## 7. Known prototype limitations (v2 TODO)

- **FLAC / Opus decode.** The `stream_decoder` stub accepts codec IDs but
  returns `ESP_ERR_NOT_SUPPORTED` until the ADF `audio_pipeline` is wired.
  See `flac_decoder.c` for the reference setup.
- **LDAC / aptX.** Intentionally excluded; proprietary licenses.
- **OTA.** Partition table reserves `ota_0`/`ota_1` slots; no updater yet.
- **AVRCP commands.** Callback is registered but play/pause/next not routed
  into `source_manager`.
- **OLED menu.** UI shows status only; multi-page menu and SSID picker TBD.
- **Coexistence tuning.** WiFi + BT classic share the radio; if you see
  audio dropouts during WiFi activity, enable `CONFIG_ESP_COEX_SW_COEXIST`.
- **Power management.** Light-sleep disabled to keep I²S running cleanly.
- **External I²S sources (HDMI ARC / Toslink / USB).** Will be attached as
  I²S slave daughter boards; firmware gains an `SRC_I2S_EXT` enum value
  and a GPIO-based mux.

## 8. License

Firmware is BSD-3-Clause. Bundled fonts and Ben Buxton encoder FSM are
public domain / BSD. FLAC (BSD), Opus (BSD), SBC (royalty-free) are used
via ESP-ADF; AAC decode is expected to be license-safe post-2023.

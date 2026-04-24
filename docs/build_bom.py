"""Generate the SA32 Bill of Materials as an XLSX file.

The component list is derived from smart_amp_proto/README.md (verified CZ/EU
parts table) and the wiring section. Prices are typical 2026 retail estimates
in CZK (single-unit) — useful for a budget, not a quote.
"""

from openpyxl import Workbook
from openpyxl.styles import Alignment, Font, PatternFill, Border, Side
from openpyxl.utils import get_column_letter

OUT = "/Users/tomasurl/AS32/AS32/docs/SA32_BOM.xlsx"

# (category, qty, ref, part, package/notes, suppliers, est_unit_price_czk)
COMPONENTS = [
    # ---- Core compute ----
    ("MCU",        1, "U1", "ESP32-S3-DevKitC-1 (WROOM-1-N16R8, 16 MB flash, 8 MB octal PSRAM)",
     "DevKit modul, USB-C", "Mouser.cz, DigiKey.cz, LaskaKit.cz, PUHY.cz", 650),

    # ---- Audio chain ----
    ("Audio",      1, "U2", "PCM5102A I²S DAC breakout (SCK→GND, line out 2.1 Vrms)",
     "modul ~38×25 mm",     "LaskaKit.cz, TinyTronics.nl, AliExpress EU", 220),
    ("Audio",      1, "U3", "TPA3110D2 2×15 W class-D zesilovač modul",
     "PBTL/stereo DIP zkratovací, 12–24 V vstup", "AliExpress EU, GM Electronic", 220),
    ("Audio",      1, "J1", "Stereo 3.5 mm jack PCB (vstup z DAC do AMP)",
     "stíněný kablík ~10 cm", "GM Electronic, TME.eu", 35),

    # ---- UI ----
    ("UI",         1, "U4", "OLED displej SSD1306 128×64 I²C, adresa 0x3C, 0,96\"",
     "VCC 3,3 V, 4-pin (SDA/SCL/VCC/GND)", "Robotstore.cz, Pajtech.cz, LaskaKit.cz", 130),
    ("UI",         1, "S1", "Rotační enkodér Alps EC11 (24 pulzů, taktilní switch)",
     "vyveden A/B/SW + 2× GND, hřídel 20 mm",  "Mouser.cz, Farnell.cz, TME.eu", 75),
    ("UI",         1, "K1", "Knoflík hliníkový 6 mm D-shaft, ⌀ 20 mm",
     "matný hliník",        "TME.eu, Mouser.cz", 60),

    # ---- Power ----
    ("Power",      1, "PSU","Mean Well IRM-20-24 (24 V / 20 W) nebo ekvivalent",
     "AC/DC zalitý modul",  "GM Electronic (gme.cz), Mouser.cz", 350),
    ("Power",      1, "J2", "Souosý DC jack 5,5/2,1 mm panelový",
     "lepený / matka",      "GME, TME.eu", 30),
    ("Power",      1, "U5", "AMS1117-3.3 LDO modul (vstup 5 V → 3,3 V, ≥ 500 mA)",
     "alt: napájení z USB DevKitu při dev",  "GME, LaskaKit.cz", 25),
    ("Power",      1, "U6", "AP2112-1.8 LDO (volitelné, čistá analogová větev DAC)",
     "SOT-23-5",            "Mouser.cz, Farnell.cz", 25),
    ("Power",      1, "F1", "Tavná pojistka 5×20 mm 1,6 A T + držák",
     "primární strana 230 V", "GME, TME.eu", 35),

    # ---- Pasivní ----
    ("Passive",    2, "C1,C2","Elektrolyt 1000 µF / 35 V low-ESR",
     "filtrace 24 V vstupu AMP", "GME, TME.eu", 25),
    ("Passive",    2, "C3,C4","MLCC 100 nF / 50 V X7R 0,1\"",
     "blok napájení",            "GME, TME.eu", 3),
    ("Passive",    2, "R1,R2","Pull-up 10 kΩ 1/4 W (rezerva pro I²C, pokud modul nemá)",
     "většina OLED breakoutů má vlastní", "GME", 2),
    ("Passive",    2, "R3,R4","Sériový rezistor 33 Ω I²C (volitelně, ringing fix)",
     "v případě dlouhého wiringu", "GME", 2),

    # ---- Mechanika ----
    ("Mech",       2, "SP",  "Reproduktor 8 Ω / ≥ 15 W (3–4\" full-range)",
     "podle krabičky",         "Reichelt.cz, TME.eu", 280),
    ("Mech",       1, "BOX", "Hliníková krabička ~120×100×45 mm (např. Hammond 1455)",
     "frézování čela pro OLED + enkodér", "TME.eu, Mouser.cz", 320),
    ("Mech",       1, "WIRE","Sada propojovacích vodičů Dupont F-F + silnější 18 AWG (PSU/SPK)",
     "mix",                    "GME, AliExpress",        80),
    ("Mech",       1, "STAND","Stojánky M3 mosaz 8 mm + šrouby/matky (4 ks DAC, 4 ks AMP, 4 ks DevKit)",
     "izolace od šasi",        "GME, TME.eu",            45),

    # ---- Volitelné/budoucí ----
    ("Optional",   1, "U7", "PCM5102A XSMT vyvedení (digi mute) — pokud modul má pad",
     "GPIO13, Kconfig SOKOL_DAC_MUTE_PIN_ENABLE", "—", 0),
    ("Optional",   2, "LED","Status LED 3 mm + 470 Ω (WiFi/BT) — GPIO38/39",
     "Kconfig SOKOL_STATUS_LEDS_ENABLE",          "GME", 8),
]

THIN = Side(border_style="thin", color="BFBFBF")
BORDER = Border(top=THIN, bottom=THIN, left=THIN, right=THIN)
HEADER_FILL = PatternFill("solid", fgColor="0D9488")
SECTION_FILL = PatternFill("solid", fgColor="E2E8F0")
TOTAL_FILL = PatternFill("solid", fgColor="1D4ED8")

def main():
    wb = Workbook()

    # ---- Sheet 1: BOM ----
    ws = wb.active
    ws.title = "BOM"
    headers = ["#", "Kategorie", "Ks", "Ozn.", "Součástka",
               "Pouzdro / poznámka", "Dodavatelé (CZ/EU)",
               "Cena/ks (CZK)", "Cena celkem (CZK)"]
    ws.append(headers)
    for col, _ in enumerate(headers, start=1):
        cell = ws.cell(row=1, column=col)
        cell.font = Font(bold=True, color="FFFFFF")
        cell.fill = HEADER_FILL
        cell.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)
        cell.border = BORDER

    total = 0
    for i, (cat, qty, ref, part, pkg, sup, price) in enumerate(COMPONENTS, start=1):
        line_total = qty * price
        total += line_total
        ws.append([i, cat, qty, ref, part, pkg, sup, price, line_total])
        for col in range(1, len(headers) + 1):
            c = ws.cell(row=i + 1, column=col)
            c.border = BORDER
            c.alignment = Alignment(vertical="top", wrap_text=True)
            if cat == "Optional":
                c.fill = SECTION_FILL

    last = ws.max_row
    ws.append([])
    ws.cell(row=last + 2, column=8, value="CELKEM (orient.)").font = Font(bold=True)
    total_cell = ws.cell(row=last + 2, column=9, value=total)
    total_cell.font = Font(bold=True, color="FFFFFF")
    total_cell.fill = TOTAL_FILL
    total_cell.border = BORDER

    widths = [4, 11, 4, 8, 46, 38, 36, 14, 16]
    for i, w in enumerate(widths, start=1):
        ws.column_dimensions[get_column_letter(i)].width = w
    ws.row_dimensions[1].height = 32
    ws.freeze_panes = "A2"

    # ---- Sheet 2: GPIO map ----
    g = wb.create_sheet("GPIO map")
    gh = ["GPIO", "Funkce", "Směr", "Pull / IRQ", "Připojení", "Pozn."]
    g.append(gh)
    for col in range(1, len(gh) + 1):
        c = g.cell(row=1, column=col)
        c.font = Font(bold=True, color="FFFFFF")
        c.fill = HEADER_FILL
        c.alignment = Alignment(horizontal="center")
        c.border = BORDER

    GPIO = [
        (5,  "I²S BCLK",     "out", "—",                "PCM5102A BCK",        "44,1 kHz × 32 = 1,4112 MHz"),
        (6,  "I²S LRCK",     "out", "—",                "PCM5102A LCK",        "WS, 44,1 kHz"),
        (7,  "I²S DOUT",     "out", "—",                "PCM5102A DIN",        "S16LE stereo"),
        (15, "I²S MCLK (opt)","out","—",                "PCM5102A SCK",        "Kconfig SOKOL_DAC_MCLK_ENABLE"),
        (8,  "I²C SDA",      "io",  "pull-up int.",     "SSD1306 SDA",         "400 kHz, sdílí budoucí I²C"),
        (9,  "I²C SCL",      "out", "pull-up int.",     "SSD1306 SCL",         "400 kHz"),
        (10, "Encoder A",    "in",  "PU + IRQ any-edge","EC11 A",              "FSM v rotary_ec11.c"),
        (11, "Encoder B",    "in",  "PU + IRQ any-edge","EC11 B",              ""),
        (12, "Encoder SW",   "in",  "PU + IRQ falling", "EC11 SW",             "active LOW, debounce SW"),
        (21, "AMP MUTE",     "out", "—",                "TPA3110D2 MUTE",      "active HIGH = ticho, default při bootu"),
        (13, "DAC XSMT (opt)","out","—",                "PCM5102A XSMT",       "Kconfig SOKOL_DAC_MUTE_PIN_ENABLE"),
        (38, "WiFi LED (opt)","out","—",                "LED → 470 Ω → GND",   "Kconfig SOKOL_STATUS_LEDS_ENABLE"),
        (39, "BT LED (opt)", "out", "—",                "LED → 470 Ω → GND",   ""),
    ]
    for row in GPIO:
        g.append(row)
        r = g.max_row
        for col in range(1, len(gh) + 1):
            g.cell(row=r, column=col).border = BORDER
            g.cell(row=r, column=col).alignment = Alignment(vertical="top", wrap_text=True)
    for i, w in enumerate([7, 16, 6, 18, 22, 42], start=1):
        g.column_dimensions[get_column_letter(i)].width = w
    g.row_dimensions[1].height = 24
    g.freeze_panes = "A2"

    # ---- Sheet 3: Power budget ----
    p = wb.create_sheet("Napájení")
    ph = ["Větev", "Napětí", "Typický odběr", "Špička", "Zdroj"]
    p.append(ph)
    for col in range(1, len(ph) + 1):
        c = p.cell(row=1, column=col)
        c.font = Font(bold=True, color="FFFFFF")
        c.fill = HEADER_FILL
        c.alignment = Alignment(horizontal="center")
        c.border = BORDER

    PWR = [
        ("AMP (TPA3110D2)",   "24 V", "150 mA (klid)",  "1,2 A (15 W RMS)",  "IRM-20-24"),
        ("ESP32-S3 DevKit",    "5 V",  "120 mA",         "350 mA (BT TX)",    "USB během vývoje, jinak 5 V z PSU větve"),
        ("ESP32-S3 jádro",     "3,3 V","—",              "—",                 "interní LDO DevKitu"),
        ("PCM5102A DAC",       "3,3 V","20 mA",          "30 mA",             "AMS1117-3.3 (sdílí s ESP)"),
        ("SSD1306 OLED",       "3,3 V","8 mA",           "20 mA (vše svítí)", "AMS1117-3.3"),
        ("EC11 enkodér",       "3,3 V","< 1 mA",         "—",                 "interní pull-upy"),
        ("Reproduktory",       "—",    "—",              "2× 15 W RMS",       "z TPA3110D2"),
    ]
    for row in PWR:
        p.append(row)
        r = p.max_row
        for col in range(1, len(ph) + 1):
            p.cell(row=r, column=col).border = BORDER
            p.cell(row=r, column=col).alignment = Alignment(vertical="top", wrap_text=True)
    for i, w in enumerate([24, 8, 18, 22, 32], start=1):
        p.column_dimensions[get_column_letter(i)].width = w
    p.row_dimensions[1].height = 24
    p.freeze_panes = "A2"

    wb.save(OUT)
    print(f"Wrote {OUT}")

if __name__ == "__main__":
    main()

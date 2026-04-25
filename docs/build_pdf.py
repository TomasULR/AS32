"""Generate the SA32 v1.1 assembly + validation manual.

Layout:
  1. Title
  2. Co je nového v v1.1 (changelog vs Phase 1)
  3. Bezpečnost + nářadí
  4. Postup sestavení (kroky 1-10)
  5. Schéma zapojení (signal flow + power tree)  — landscape
  6. Pinout GPIO + výpočet napájení
  7. Firmware: bring-up, CLI, ADF pipeline, AVRCP, OTA
  8. Streamování (UDP + BT)
  9. Validační report (15 kontrol)
 10. Řešení potíží
 11. Roadmap
"""

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.lib.enums import TA_LEFT, TA_CENTER
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer,
    Table, TableStyle, PageBreak, KeepTogether
)
from reportlab.platypus.doctemplate import NextPageTemplate
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.graphics.shapes import Drawing, Rect, Line, String, Polygon, Circle
import os

OUT = "/Users/tomasurl/AS32/AS32/docs/SA32_navod_sestaveni.pdf"

# ---------- Fonts ----------
FONT = "Helvetica"
FONT_BOLD = "Helvetica-Bold"
for path in (
    "/Library/Fonts/Arial Unicode.ttf",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/opt/homebrew/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
):
    if os.path.exists(path):
        try:
            pdfmetrics.registerFont(TTFont("Body", path))
            FONT = "Body"
            base = os.path.dirname(path)
            for cand in ("DejaVuSans-Bold.ttf", "Arial Unicode.ttf"):
                bp = os.path.join(base, cand)
                if os.path.exists(bp):
                    pdfmetrics.registerFont(TTFont("BodyBold", bp))
                    FONT_BOLD = "BodyBold"
                    break
            else:
                FONT_BOLD = FONT
            break
        except Exception:
            pass

PRIMARY = colors.HexColor("#0D9488")
INK     = colors.HexColor("#0F172A")
MUTED   = colors.HexColor("#475569")
LIGHT   = colors.HexColor("#E2E8F0")
WARN    = colors.HexColor("#B91C1C")
ACCENT  = colors.HexColor("#1D4ED8")
GREEN   = colors.HexColor("#16A34A")
ORANGE  = colors.HexColor("#EA580C")
PURPLE  = colors.HexColor("#7C3AED")

# ---------- Styles ----------
def st(name, **kw):
    base = dict(fontName=FONT, fontSize=10, leading=14, textColor=INK)
    base.update(kw)
    return ParagraphStyle(name=name, **base)

S_TITLE = st("Title", fontName=FONT_BOLD, fontSize=26, leading=30, textColor=PRIMARY, alignment=TA_CENTER)
S_SUB   = st("Sub",   fontSize=14, leading=18, textColor=MUTED, alignment=TA_CENTER)
S_H1    = st("H1",    fontName=FONT_BOLD, fontSize=18, leading=22, textColor=PRIMARY, spaceBefore=10, spaceAfter=8)
S_H2    = st("H2",    fontName=FONT_BOLD, fontSize=13, leading=16, textColor=INK,    spaceBefore=8, spaceAfter=4)
S_BODY  = st("Body")
S_LI    = st("LI",    leftIndent=14, bulletIndent=2)
S_NOTE  = st("Note",  fontSize=9, leading=12, textColor=MUTED)
S_WARN  = st("Warn",  fontName=FONT_BOLD, textColor=WARN)
S_CODE  = st("Code",  fontName="Courier", fontSize=9, leading=12, textColor=INK,
              backColor=colors.HexColor("#F1F5F9"))
S_BAD   = st("Bad",   fontName=FONT_BOLD, fontSize=10, textColor=WARN)
S_GOOD  = st("Good",  fontName=FONT_BOLD, fontSize=10, textColor=GREEN)

# ---------- Page chrome ----------
def header_footer(canv, doc):
    canv.saveState()
    canv.setFillColor(PRIMARY)
    canv.rect(0, A4[1] - 12 * mm, A4[0], 6 * mm, fill=1, stroke=0)
    canv.setFont(FONT_BOLD, 9); canv.setFillColor(colors.white)
    canv.drawString(15 * mm, A4[1] - 9.5 * mm, "SA32 v1.1 — návod, validace a firmware")
    canv.drawRightString(A4[0] - 15 * mm, A4[1] - 9.5 * mm, "rev v1.1.0")
    canv.setFillColor(MUTED); canv.setFont(FONT, 8)
    canv.drawCentredString(A4[0] / 2, 10 * mm, f"strana {doc.page}")
    canv.restoreState()

def header_footer_landscape(canv, doc):
    W, H = landscape(A4)
    canv.saveState()
    canv.setFillColor(PRIMARY)
    canv.rect(0, H - 12 * mm, W, 6 * mm, fill=1, stroke=0)
    canv.setFont(FONT_BOLD, 9); canv.setFillColor(colors.white)
    canv.drawString(15 * mm, H - 9.5 * mm, "SA32 v1.1 — schéma zapojení")
    canv.drawRightString(W - 15 * mm, H - 9.5 * mm, "rev v1.1.0")
    canv.setFillColor(MUTED); canv.setFont(FONT, 8)
    canv.drawCentredString(W / 2, 10 * mm, f"strana {doc.page}")
    canv.restoreState()

def cover(canv, doc):
    canv.saveState()
    canv.setFillColor(PRIMARY)
    canv.rect(0, A4[1] - 70 * mm, A4[0], 70 * mm, fill=1, stroke=0)
    canv.setFillColor(colors.white)
    canv.setFont(FONT_BOLD, 36)
    canv.drawCentredString(A4[0] / 2, A4[1] - 40 * mm, "SA32 v1.1")
    canv.setFont(FONT, 14)
    canv.drawCentredString(A4[0] / 2, A4[1] - 52 * mm, "Smart Hi-Fi Amp — Sériová verze")
    canv.setFont(FONT, 11)
    canv.drawCentredString(A4[0] / 2, A4[1] - 62 * mm,
        "ESP32-S3 + BK3266 BT  •  HW I²S MUX  •  PCM5102A → TPA3110D2  •  ADF + OTA")

    canv.setFillColor(INK); canv.setFont(FONT_BOLD, 22)
    canv.drawCentredString(A4[0] / 2, A4[1] - 110 * mm, "Návod, validace a firmware")
    canv.setFillColor(MUTED); canv.setFont(FONT, 11)
    canv.drawCentredString(A4[0] / 2, A4[1] - 122 * mm,
        "Kompletní výpočet obvodu • aktualizovaný HW návrh • implementace BT/ADF/OTA")

    canv.setStrokeColor(LIGHT); canv.setFillColor(colors.HexColor("#F8FAFC"))
    canv.roundRect(20 * mm, 25 * mm, A4[0] - 40 * mm, 70 * mm, 6, fill=1, stroke=1)
    canv.setFillColor(INK); canv.setFont(FONT_BOLD, 11)
    canv.drawString(28 * mm, 85 * mm, "Co je nového v v1.1:")
    canv.setFont(FONT, 10); canv.setFillColor(INK)
    items = [
        "•  Externí BT receiver (BK3266) přes UART AT — řeší absenci BR/EDR v ESP32-S3",
        "•  Hardware I²S MUX (NX3L4684) místo software bridge — bez latence",
        "•  Mean Well IRM-45-24 (1,88 A) — 30 % rezerva nad TPA3110D2 peak",
        "•  HLK-5M05 izolovaný buck 24 V → 5 V — standalone provoz bez USB-C",
        "•  ADF audio_pipeline pro FLAC + Opus dekódování (Phase 1 byl jen RAW PCM)",
        "•  AVRCP routing přes UART (play / pause / next / volume z telefonu i enkodéru)",
        "•  HTTPS OTA s rollback ochranou (cert bundle, esp_https_ota)",
    ]
    y = 75 * mm
    for it in items:
        canv.drawString(28 * mm, y, it); y -= 5.5 * mm
    canv.setFillColor(MUTED); canv.setFont(FONT, 9)
    canv.drawString(28 * mm, 30 * mm,
        "Cílový HW: ESP32-S3-DevKitC-1 N16R8  •  Licence firmware: BSD-3-Clause  •  Datum: 2026-04")
    canv.restoreState()

# ---------- Helpers ----------
def li(text):
    return Paragraph(f"• {text}", S_LI)

def step(num, title, paras):
    flow = [Paragraph(f"<b>Krok {num}.</b> &nbsp; {title}", S_H2)]
    for p in paras:
        flow.append(Paragraph(p, S_BODY))
    flow.append(Spacer(1, 6))
    return KeepTogether(flow)

def small_table(rows, col_widths, status_col=None):
    cell_style = ParagraphStyle("cell", fontName=FONT, fontSize=9, leading=11, textColor=INK)
    head_style = ParagraphStyle("head", fontName=FONT_BOLD, fontSize=9, leading=11, textColor=colors.white)
    wrapped = []
    for ri, row in enumerate(rows):
        new_row = []
        for ci, cell in enumerate(row):
            if ri == 0:
                new_row.append(Paragraph(str(cell), head_style))
            elif status_col is not None and ci == status_col:
                txt = str(cell)
                color = "#047857" if txt == "OK" else ("#B45309" if txt in ("BORDERLINE", "ACTION") else
                       ("#1D4ED8" if txt == "DOC" else "#0F172A"))
                style = ParagraphStyle("st", fontName=FONT_BOLD, fontSize=9,
                                       leading=11, textColor=colors.HexColor(color))
                new_row.append(Paragraph(txt, style))
            else:
                new_row.append(Paragraph(str(cell), cell_style))
        wrapped.append(new_row)
    t = Table(wrapped, colWidths=col_widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("FONT", (0, 0), (-1, -1), FONT, 9),
        ("BACKGROUND", (0, 0), (-1, 0), PRIMARY),
        ("BOX", (0, 0), (-1, -1), 0.4, MUTED),
        ("INNERGRID", (0, 0), (-1, -1), 0.25, LIGHT),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]))
    return t

# ---------- Wiring diagram (v1.1: 2-pane layout) ----------
def wiring_diagram():
    """760 × 540 pt — dva panely: signal flow nahoře, power tree dole.
    Vykresluje se v interním 540 px souřadném systému, výsledek scale-down na 490 px aby se vešel do landscape A4 frame."""
    d = Drawing(760, 490)
    d.transform = (1.0, 0, 0, 490.0/540.0, 0, 0)   # uniform-X, scale-Y down

    # Title
    d.add(String(0, 525, "Schéma zapojení v1.1 — signal flow + power tree",
                 fontName=FONT_BOLD, fontSize=14, fillColor=PRIMARY))
    d.add(String(0, 510,
                 "Šipky jdou od zdroje signálu k cíli. Volitelné spoje jsou čárkovaně. "
                 "Barva drátu = třída signálu (viz legenda).",
                 fontName=FONT, fontSize=8, fillColor=MUTED))

    # Helper closures
    def box(x, y, w, h, label, sub="", fill="#E0F2F1", stroke=PRIMARY):
        d.add(Rect(x, y, w, h, fillColor=colors.HexColor(fill),
                   strokeColor=stroke, strokeWidth=1.2, rx=4, ry=4))
        d.add(String(x + w / 2, y + h - 12, label, fontName=FONT_BOLD,
                     fontSize=10, fillColor=INK, textAnchor="middle"))
        if sub:
            d.add(String(x + w / 2, y + h - 24, sub, fontName=FONT,
                         fontSize=7, fillColor=MUTED, textAnchor="middle"))

    def pin(x, y, label, anchor="start", color=INK):
        d.add(String(x, y, label, fontName="Courier", fontSize=7,
                     fillColor=color, textAnchor=anchor))

    def wire(x1, y1, x2, y2, color=ACCENT, dashed=False, width=1.2):
        ln = Line(x1, y1, x2, y2, strokeColor=color, strokeWidth=width)
        if dashed:
            ln.strokeDashArray = [3, 2]
        d.add(ln)

    def arrow(x, y, color=ACCENT):
        s = 4
        d.add(Polygon(points=[x, y, x - s, y - s/1.5, x - s, y + s/1.5],
                      fillColor=color, strokeColor=color))

    # =============================================================
    # TOP HALF (y=290..490): SIGNAL FLOW
    # =============================================================
    d.add(String(0, 490, "SIGNAL FLOW", fontName=FONT_BOLD, fontSize=10, fillColor=ACCENT))

    # ESP32-S3 box (left). Pins jen jako značka + jméno funkce, ne dvojité GPIO+fn
    esp_x, esp_y, esp_w, esp_h = 40, 285, 180, 195
    box(esp_x, esp_y, esp_w, esp_h, "ESP32-S3-DevKitC-1",
        "WROOM-1 N16R8  •  USB-C", fill="#ECFEFF")
    # Left pins (I²S out → MUX, I²C → OLED) — jen funkční jméno uvnitř boxu
    left_pins = [
        ("BCLK (5)",   esp_y + esp_h - 35),
        ("LRCK (6)",   esp_y + esp_h - 48),
        ("DOUT (7)",   esp_y + esp_h - 61),
        ("MUX_SEL (4)", esp_y + esp_h - 80),
        ("MUTE (21)",  esp_y + esp_h - 93),
        ("SDA (8)",    esp_y + esp_h - 112),
        ("SCL (9)",    esp_y + esp_h - 125),
    ]
    for fn, y in left_pins:
        pin(esp_x + 6, y - 2, fn)
        d.add(Circle(esp_x, y, 2, fillColor=INK, strokeColor=INK))
    # Right pins
    right_pins = [
        ("(10) ENC_A",   esp_y + esp_h - 35),
        ("(11) ENC_B",   esp_y + esp_h - 48),
        ("(12) ENC_SW",  esp_y + esp_h - 61),
        ("(14) BK_RST",  esp_y + esp_h - 80),
        ("(17) UART_TX", esp_y + esp_h - 93),
        ("(18) UART_RX", esp_y + esp_h - 106),
    ]
    for fn, y in right_pins:
        pin(esp_x + esp_w - 6, y - 2, fn, anchor="end")
        d.add(Circle(esp_x + esp_w, y, 2, fillColor=INK, strokeColor=INK))

    # I²S MUX (center) — y nižší aby BK3266 nahoře nezasahoval do titulu
    mux_x, mux_y, mux_w, mux_h = 290, 310, 110, 100
    box(mux_x, mux_y, mux_w, mux_h, "I²S MUX", "NX3L4684 / 74HC4053", fill="#FEF3C7")
    pin(mux_x + 3, mux_y + mux_h - 32, "ESP_BCK")
    pin(mux_x + 3, mux_y + mux_h - 42, "ESP_LRC")
    pin(mux_x + 3, mux_y + mux_h - 52, "ESP_SDA")
    pin(mux_x + 3, mux_y + mux_h - 65, "BT_BCK")
    pin(mux_x + 3, mux_y + mux_h - 75, "BT_LRC")
    pin(mux_x + 3, mux_y + mux_h - 85, "BT_SDA")
    pin(mux_x + mux_w / 2, mux_y + 3, "SEL", anchor="middle")
    pin(mux_x + mux_w - 3, mux_y + mux_h - 50, "OUT_BCK", anchor="end")
    pin(mux_x + mux_w - 3, mux_y + mux_h - 60, "OUT_LRC", anchor="end")
    pin(mux_x + mux_w - 3, mux_y + mux_h - 70, "OUT_SDA", anchor="end")

    # ESP I²S (3 wires) → MUX
    for i, esp_pin_y in enumerate([esp_y + esp_h - 35, esp_y + esp_h - 48, esp_y + esp_h - 61]):
        target_y = mux_y + mux_h - 32 - i * 10
        wire(esp_x, esp_pin_y, mux_x, target_y)
        arrow(mux_x, target_y)

    # GPIO 4 → MUX SEL (orange)
    wire(esp_x, esp_y + esp_h - 80, mux_x + mux_w / 2, mux_y, color=ORANGE, width=1.3)
    arrow(mux_x + mux_w / 2, mux_y, color=ORANGE)

    # BK3266 BT receiver — umístěn nad MUX (sdílí horizontál y), pins:
    #   bottom = I²S out (BCK/LRC/SDA, dolů přímo do MUX top)
    #   right  = UART control (RX/TX/RST z ESP)
    bk_x, bk_y, bk_w, bk_h = 290, 420, 230, 50
    box(bk_x, bk_y, bk_w, bk_h, "BK3266 BT receiver",
        "BT 5.0 BR/EDR + A2DP/AVRCP  •  I²S out + UART control", fill="#F3E8FF")
    # Bottom pins (I²S out → MUX BT inputs)
    pin(bk_x + 30,  bk_y + 4, "BCK_B")
    pin(bk_x + 80,  bk_y + 4, "LRC_B")
    pin(bk_x + 130, bk_y + 4, "SDA_B")
    # Right pins (UART)
    pin(bk_x + bk_w - 3, bk_y + 35, "RST", anchor="end")
    pin(bk_x + bk_w - 3, bk_y + 25, "RX",  anchor="end")
    pin(bk_x + bk_w - 3, bk_y + 15, "TX",  anchor="end")

    # ESP UART2 + RST → BK3266 (purple) — z pravé strany ESP nahoru a do BK pravé strany
    uart_x = bk_x + bk_w + 10  # vertikální koridor vpravo od BK
    wire(esp_x + esp_w, esp_y + esp_h - 80,  uart_x, esp_y + esp_h - 80,  color=PURPLE, width=1.2)
    wire(esp_x + esp_w, esp_y + esp_h - 93,  uart_x - 4, esp_y + esp_h - 93, color=PURPLE, width=1.2)
    wire(esp_x + esp_w, esp_y + esp_h - 106, uart_x - 8, esp_y + esp_h - 106, color=PURPLE, width=1.2)
    # Vertikální koridor pak nahoru do BK pravé strany
    wire(uart_x,     esp_y + esp_h - 80,  uart_x,     bk_y + 35, color=PURPLE, width=1.2)
    wire(uart_x - 4, esp_y + esp_h - 93,  uart_x - 4, bk_y + 25, color=PURPLE, width=1.2)
    wire(uart_x - 8, esp_y + esp_h - 106, uart_x - 8, bk_y + 15, color=PURPLE, width=1.2)
    wire(uart_x,     bk_y + 35, bk_x + bk_w, bk_y + 35, color=PURPLE, width=1.2)
    wire(uart_x - 4, bk_y + 25, bk_x + bk_w, bk_y + 25, color=PURPLE, width=1.2)
    wire(uart_x - 8, bk_y + 15, bk_x + bk_w, bk_y + 15, color=PURPLE, width=1.2)
    arrow(bk_x + bk_w, bk_y + 35, color=PURPLE)
    arrow(bk_x + bk_w, bk_y + 25, color=PURPLE)
    arrow(bk_x + bk_w, bk_y + 15, color=PURPLE)
    d.add(String(uart_x + 4, (esp_y + esp_h + bk_y) / 2, "UART2 + RST",
                 fontName=FONT, fontSize=7, fillColor=PURPLE))

    # BK3266 I²S OUT (bottom) → MUX BT INPUTS (top of MUX, BT_BCK/BT_LRC/BT_SDA)
    # MUX top edge at y = mux_y + mux_h. BT pins ale jsou v originálním layoutu na levé straně.
    # Přesunu BT pins na MUX TOP — overrides předchozí pin labels.
    pin(mux_x + 25,  mux_y + mux_h - 4, "BT_BCK", anchor="middle")
    pin(mux_x + 55,  mux_y + mux_h - 4, "BT_LRC", anchor="middle")
    pin(mux_x + 85,  mux_y + mux_h - 4, "BT_SDA", anchor="middle")
    # Mažeme vizuálně původní BT pin labely uvnitř boxu malou bílou maskou (rect)
    d.add(Rect(mux_x + 1, mux_y + mux_h - 90, 50, 30,
               fillColor=colors.HexColor("#FEF3C7"), strokeColor=colors.HexColor("#FEF3C7")))
    # Přidáme zpět jen ESP_xxx labely
    pin(mux_x + 3, mux_y + mux_h - 32, "ESP_BCK")
    pin(mux_x + 3, mux_y + mux_h - 42, "ESP_LRC")
    pin(mux_x + 3, mux_y + mux_h - 52, "ESP_SDA")
    # Wires BK bottom → MUX top
    for bx_off, mx_off in [(30, 25), (80, 55), (130, 85)]:
        wire(bk_x + bx_off, bk_y, mux_x + mx_off, mux_y + mux_h)
        arrow(mux_x + mx_off, mux_y + mux_h)

    # PCM5102A DAC (right of MUX)
    dac_x, dac_y, dac_w, dac_h = 460, 320, 150, 80
    box(dac_x, dac_y, dac_w, dac_h, "PCM5102A DAC",
        "I²S in  •  Line-out 2,1 Vrms", fill="#DBEAFE")
    pin(dac_x + 3, dac_y + dac_h - 30, "BCK")
    pin(dac_x + 3, dac_y + dac_h - 42, "LCK")
    pin(dac_x + 3, dac_y + dac_h - 54, "DIN")
    pin(dac_x + dac_w - 3, dac_y + 10, "L/R OUT", anchor="end")

    # MUX → DAC (3 wires, blue)
    for i in range(3):
        my = mux_y + mux_h - 50 - i * 10
        dy = dac_y + dac_h - 30 - i * 12
        wire(mux_x + mux_w, my, dac_x, dy)
        arrow(dac_x, dy)

    # OLED (top-left, near ESP)
    oled_x, oled_y, oled_w, oled_h = 40, 235, 110, 50
    box(oled_x, oled_y, oled_w, oled_h, "SSD1306 OLED",
        "I²C 0x3C  •  3,3 V", fill="#F3E8FF")
    pin(oled_x + 3, oled_y + 24, "SDA")
    pin(oled_x + 3, oled_y + 12, "SCL")
    # I²C from ESP
    wire(esp_x + 30, esp_y, esp_x + 30, oled_y + oled_h)
    wire(esp_x + 30, oled_y + 24, oled_x, oled_y + 24)
    arrow(oled_x, oled_y + 24)
    wire(esp_x + 30, oled_y + 12, oled_x, oled_y + 12)
    arrow(oled_x, oled_y + 12)

    # EC11 encoder (right of ESP)
    enc_x, enc_y, enc_w, enc_h = 290, 250, 110, 70
    box(enc_x, enc_y, enc_w, enc_h, "EC11 enkodér",
        "rotace + tlačítko", fill="#FCE7F3")
    pin(enc_x + 3, enc_y + 40, "A")
    pin(enc_x + 3, enc_y + 28, "B")
    pin(enc_x + 3, enc_y + 16, "SW")
    wire(esp_x + esp_w, esp_y + esp_h - 35, enc_x, enc_y + 40)
    arrow(enc_x, enc_y + 40)
    wire(esp_x + esp_w, esp_y + esp_h - 48, enc_x, enc_y + 28)
    arrow(enc_x, enc_y + 28)
    wire(esp_x + esp_w, esp_y + esp_h - 61, enc_x, enc_y + 16)
    arrow(enc_x, enc_y + 16)

    # AMP TPA3110D2 (right of DAC, lower)
    amp_x, amp_y, amp_w, amp_h = 460, 220, 150, 90
    box(amp_x, amp_y, amp_w, amp_h, "TPA3110D2",
        "2× 15 W class-D  •  GAIN=12 dB", fill="#FEF3C7")
    pin(amp_x + 22, amp_y + amp_h - 20, "L/R IN")
    pin(amp_x + 22, amp_y + amp_h - 50, "MUTE")
    pin(amp_x + amp_w - 22, amp_y + amp_h - 20, "SPK L+/-", anchor="end")
    pin(amp_x + amp_w - 22, amp_y + amp_h - 50, "SPK R+/-", anchor="end")
    pin(amp_x + amp_w - 22, amp_y + 20, "VIN 24 V", anchor="end")

    # DAC → AMP (analog, green)
    wire(dac_x + dac_w - 30, dac_y, dac_x + dac_w - 30, amp_y + amp_h - 30,
         color=GREEN, width=1.6)
    wire(dac_x + dac_w - 30, amp_y + amp_h - 30, amp_x + 6, amp_y + amp_h - 30,
         color=GREEN, width=1.6)
    arrow(amp_x + 6, amp_y + amp_h - 30, color=GREEN)
    d.add(String(dac_x + dac_w - 25, dac_y - 12, "analog 2,1 Vrms",
                 fontName=FONT, fontSize=7, fillColor=GREEN, textAnchor="start"))

    # ESP MUTE → AMP
    wire(esp_x + 30, esp_y + esp_h - 93, esp_x + 30, amp_y + amp_h - 60)
    wire(esp_x + 30, amp_y + amp_h - 60, amp_x + 6, amp_y + amp_h - 60,
         color=WARN, width=1.4)
    arrow(amp_x + 6, amp_y + amp_h - 60, color=WARN)

    # SPK
    spk_x, spk_y, spk_w, spk_h = 660, 220, 80, 90
    box(spk_x, spk_y, spk_w, spk_h, "SPK 8 Ω", "2× full-range",
        fill="#E2E8F0", stroke=MUTED)
    wire(amp_x + amp_w, amp_y + amp_h - 20, spk_x, spk_y + 70, color=GREEN, width=1.6)
    arrow(spk_x, spk_y + 70, color=GREEN)
    wire(amp_x + amp_w, amp_y + amp_h - 50, spk_x, spk_y + 30, color=GREEN, width=1.6)
    arrow(spk_x, spk_y + 30, color=GREEN)

    # =============================================================
    # SEPARATOR
    # =============================================================
    d.add(Line(0, 200, 760, 200, strokeColor=MUTED, strokeWidth=0.5,
               strokeDashArray=[2, 2]))
    d.add(String(0, 187, "POWER TREE", fontName=FONT_BOLD, fontSize=10, fillColor=WARN))
    d.add(String(0, 175,
                 "Star ground na výstupu PSU. Single-point junction; oddělené vodiče pro AMP power, logiku a audio.",
                 fontName=FONT, fontSize=8, fillColor=MUTED))

    # =============================================================
    # BOTTOM HALF (y=20..170): POWER TREE
    # =============================================================
    # IRM-45-24 PSU
    psu_x, psu_y, psu_w, psu_h = 40, 80, 170, 70
    box(psu_x, psu_y, psu_w, psu_h, "Mean Well IRM-45-24",
        "230 V AC → 24 V DC / 1,88 A", fill="#FEE2E2")
    pin(psu_x + 3, psu_y + 35, "L / N (230V)")
    pin(psu_x + psu_w - 3, psu_y + 45, "+24 V", anchor="end")
    pin(psu_x + psu_w - 3, psu_y + 25, "GND", anchor="end")

    # 24V rail to AMP (red, up to top half)
    wire(psu_x + psu_w, psu_y + 45, psu_x + psu_w + 15, psu_y + 45, color=WARN, width=1.6)
    wire(psu_x + psu_w + 15, psu_y + 45, psu_x + psu_w + 15, amp_y + 20, color=WARN, width=1.6)
    wire(psu_x + psu_w + 15, amp_y + 20, amp_x + amp_w - 22, amp_y + 20, color=WARN, width=1.6)
    arrow(amp_x + amp_w - 22, amp_y + 20, color=WARN)
    d.add(String(psu_x + psu_w + 18, psu_y + 60, "24 V → AMP",
                 fontName=FONT, fontSize=7, fillColor=WARN))

    # HLK-5M05 buck (24V → 5V)
    buck_x, buck_y, buck_w, buck_h = 270, 80, 130, 70
    box(buck_x, buck_y, buck_w, buck_h, "HLK-5M05 izol. buck",
        "24 V → 5 V / 1 A  •  η ≈ 75 %", fill="#FFEDD5")
    pin(buck_x + 3, buck_y + 35, "VIN 24V")
    pin(buck_x + buck_w - 3, buck_y + 35, "VOUT 5V", anchor="end")

    # PSU → Buck (24V tap)
    wire(psu_x + psu_w, psu_y + 35, buck_x, buck_y + 35, color=WARN, width=1.4)
    arrow(buck_x, buck_y + 35, color=WARN)

    # Ferrit bead + cap network (annotation)
    d.add(String(buck_x + buck_w + 5, buck_y + 50,
                 "→ ferrit BLM31 + 47µF",
                 fontName=FONT, fontSize=7, fillColor=ORANGE))

    # AMS1117-3.3 LDO
    ldo_x, ldo_y, ldo_w, ldo_h = 460, 80, 110, 70
    box(ldo_x, ldo_y, ldo_w, ldo_h, "AMS1117-3.3",
        "5 V → 3,3 V / 200 mA", fill="#FEF9C3")
    pin(ldo_x + 3, ldo_y + 35, "VIN 5V")
    pin(ldo_x + ldo_w - 3, ldo_y + 35, "VOUT 3V3", anchor="end")

    # Buck → LDO (5V)
    wire(buck_x + buck_w, buck_y + 35, ldo_x, ldo_y + 35, color=ORANGE, width=1.4)
    arrow(ldo_x, ldo_y + 35, color=ORANGE)
    d.add(String((buck_x + buck_w + ldo_x) / 2, buck_y + 50, "5 V",
                 fontName=FONT_BOLD, fontSize=8, fillColor=ORANGE,
                 textAnchor="middle"))

    # Buck → DevKit VIN (5V)
    wire(buck_x + buck_w / 2, buck_y, buck_x + buck_w / 2, 30, color=ORANGE, width=1.4)
    wire(buck_x + buck_w / 2, 30, esp_x + esp_w / 2, 30, color=ORANGE, width=1.4)
    wire(esp_x + esp_w / 2, 30, esp_x + esp_w / 2, esp_y, color=ORANGE,
         width=1.4, dashed=True)
    d.add(String(esp_x + esp_w / 2 - 60, 35, "5 V → DevKit VIN",
                 fontName=FONT, fontSize=7, fillColor=ORANGE))

    # LDO → 3,3 V consumers (DAC, OLED, BK3266, MUX) — purple
    consumers = [
        (dac_x + dac_w / 2, dac_y - 10, "DAC"),
        (oled_x + oled_w / 2, oled_y - 10, "OLED"),
        (bk_x + bk_w / 2, bk_y - 10, "BK3266"),
        (mux_x + mux_w / 2, mux_y - 10, "MUX"),
    ]
    for cx, cy, label in consumers:
        wire(ldo_x + ldo_w / 2, ldo_y, ldo_x + ldo_w / 2, 60,
             color=PURPLE, width=1.0, dashed=True)
        wire(ldo_x + ldo_w / 2, 60, cx, 60, color=PURPLE, width=1.0, dashed=True)
        wire(cx, 60, cx, cy, color=PURPLE, width=1.0, dashed=True)
        d.add(String(cx + 4, cy + 2, label, fontName=FONT, fontSize=7,
                     fillColor=PURPLE))

    # Pojistka F1 annotation
    d.add(String(psu_x, psu_y - 8,
                 "F1: 1,6 A T pojistka v sérii s L před PSU",
                 fontName=FONT, fontSize=7, fillColor=WARN))

    # =============================================================
    # LEGEND (bottom-right)
    # =============================================================
    lx, ly = 590, 130
    d.add(String(lx, ly, "Legenda", fontName=FONT_BOLD, fontSize=9, fillColor=INK))
    legend = [
        (ACCENT, "Digitální signál (I²S, I²C, GPIO)"),
        (GREEN,  "Analog audio (line + SPK)"),
        (WARN,   "Napájení 24 V / silnoproud"),
        (ORANGE, "Napájení 5 V"),
        (PURPLE, "Napájení 3,3 V + UART control"),
    ]
    for i, (c, txt) in enumerate(legend):
        y = ly - 12 - i * 11
        ln = Line(lx, y, lx + 16, y, strokeColor=c, strokeWidth=2)
        if i == 4:
            ln.strokeDashArray = [3, 2]
        d.add(ln)
        d.add(String(lx + 22, y - 3, txt, fontName=FONT, fontSize=7, fillColor=INK))

    return d

# ---------- Build ----------
def build():
    doc = BaseDocTemplate(
        OUT, pagesize=A4,
        leftMargin=18 * mm, rightMargin=18 * mm,
        topMargin=22 * mm, bottomMargin=18 * mm,
        title="SA32 v1.1 — návod, validace, firmware", author="SokolAudio")

    frame_p = Frame(18 * mm, 18 * mm, A4[0] - 36 * mm, A4[1] - 40 * mm, id="portrait")
    frame_cover = Frame(0, 0, A4[0], A4[1], id="cover")
    W, H = landscape(A4)
    frame_l = Frame(12 * mm, 18 * mm, W - 24 * mm, H - 30 * mm, id="landscape")

    doc.addPageTemplates([
        PageTemplate(id="cover",     frames=[frame_cover], onPage=cover, pagesize=A4),
        PageTemplate(id="portrait",  frames=[frame_p],     onPage=header_footer, pagesize=A4),
        PageTemplate(id="landscape", frames=[frame_l],     onPage=header_footer_landscape,
                     pagesize=landscape(A4)),
    ])

    story = []

    # ---- Cover ----
    story.append(Spacer(1, 1))
    story.append(NextPageTemplate("portrait"))
    story.append(PageBreak())

    # ---- 1. Co je nového v v1.1 ----
    story.append(Paragraph("1. Co je nového v v1.1", S_H1))
    story.append(Paragraph(
        "Phase 1 prototyp běžel s native ESP32 BT A2DP, ale verze SA32 používá "
        "ESP32-<b>S3</b>, který hardwarově nepodporuje BR/EDR (Classic) Bluetooth. "
        "Tato revize řeší tři kritické nálezy z HW review a integruje plnou audio + "
        "OTA podporu.", S_BODY))

    story.append(Paragraph("Tři opravené HW chyby Phase 1", S_H2))
    chgs = [
        ["Chyba", "Phase 1", "v1.1 řešení"],
        ["BT A2DP", "native ESP32-S3 (nefunguje, chybí BR/EDR rádio)",
         "Externí BK3266 modul s I²S výstupem, řízený přes UART AT"],
        ["PSU dimenze",
         "Mean Well IRM-20-24 (0,83 A) — restart při basových transientech 1,44 A",
         "Mean Well IRM-45-24 (1,88 A) — 30 % rezerva"],
        ["Logika napájení",
         "Závislé na USB-C; standalone bez napájení",
         "HLK-5M05 izolovaný buck 24 V → 5 V → DevKit VIN"],
    ]
    story.append(small_table(chgs, [40 * mm, 60 * mm, 70 * mm]))
    story.append(Spacer(1, 8))

    story.append(Paragraph("Tři nové funkce", S_H2))
    feats = [
        ["Funkce", "Implementace"],
        ["ADF audio_pipeline pro FLAC + Opus",
         "<code>codecs/stream_decoder.c</code>: raw_in (writer) → decoder → raw_out (reader) → pump task → i2s_dma. Přepínání kodeků za běhu (RAW PCM ↔ FLAC ↔ Opus)."],
        ["AVRCP routing (play/pause/next/volume)",
         "<code>network/bt_a2dp_sink.c</code>: UART parser BK3266 status zpráv. Enkodér push toggluje BT play/pause přes <code>AT+CC/CD</code>; otáčení volá <code>AT+VOL+/-</code>."],
        ["HTTPS OTA s rollback",
         "<code>network/ota_updater.c</code>: <code>esp_https_ota</code> + <code>MBEDTLS_CERTIFICATE_BUNDLE</code> + <code>BOOTLOADER_APP_ROLLBACK_ENABLE</code>. CLI: <code>ota &lt;url&gt;</code>. Boot validate po 30 s běhu."],
    ]
    story.append(small_table(feats, [55 * mm, 115 * mm]))

    story.append(PageBreak())

    # ---- 2. Bezpečnost & nářadí ----
    story.append(Paragraph("2. Bezpečnost a nářadí", S_H1))
    story.append(Paragraph("Před zahájením práce", S_H2))
    story.append(Paragraph(
        "Mean Well IRM-45-24 napájíš 230 V AC. <b>Veškerou práci s primární stranou</b> "
        "PSU prováděj odpojený od zásuvky. Sekundární 24 V DC je bezpečné napětí, "
        "ale dodává až 1,88 A — krátký spoj na výstupu TPA3110D2 spolehlivě zničí čip "
        "i s vestavěnou ochranou.", S_BODY))
    story.append(Spacer(1, 4))
    story.append(Paragraph(
        "<b>Důsledně dodržuj pořadí:</b> nejprve kompletně zapojit, multimetrem zkontrolovat "
        "neexistenci zkratů (24 V × GND, 5 V × GND, 3,3 V × GND), pak teprve napájet.", S_WARN))
    story.append(Spacer(1, 8))

    story.append(Paragraph("Doporučené nářadí", S_H2))
    for it in [
        "Páječka 30–60 W s hrotem ⌀ 1 mm, cín 0,5–0,8 mm s tavidlem",
        "Multimetr (kontinuita, DC napětí 0–30 V, AC 200 V)",
        "Křížový šroubovák PH0/PH1, plochý 2,5 mm na svorkovnice TPA3110D2",
        "Kleštičky na odizolování + štípačky",
        "Smršťovací bužírka 2 / 3 / 5 mm + horkovzdušná pistole",
        "USB-C kabel pro flashování DevKitu",
        "Doporučeně: <b>laboratorní zdroj s proudovým limitem 500 mA</b> pro první test (chrání AMP)",
    ]:
        story.append(li(it))

    story.append(PageBreak())

    # ---- 3. Postup sestavení ----
    story.append(Paragraph("3. Postup sestavení (krok za krokem)", S_H1))

    story.append(step(1, "Příprava DevKitu",
        ["Osaď ESP32-S3-DevKitC-1 do nepájivého pole, nebo natrvalo připájej "
         "kolíkové lišty <b>směrem dolů</b> tak, aby USB-C směřoval k zadní stěně.",
         "Zkontroluj, že je nasazený jumper EN-Reset a žádný pin není ohnutý."]))

    story.append(step(2, "I²S sběrnice ESP → MUX → DAC",
        ["Propoj ESP32 piny: <b>GPIO 5 → MUX BCK_E</b>, <b>GPIO 6 → MUX LRC_E</b>, "
         "<b>GPIO 7 → MUX SDA_E</b>. Tři kroucené páry max. 8 cm.",
         "MUX výstupy: <b>OUT_BCK → DAC BCK</b>, <b>OUT_LRC → DAC LCK</b>, "
         "<b>OUT_SDA → DAC DIN</b>.",
         "Pin SCK na PCM5102A nech spojený se zemí (interní PLL)."]))

    story.append(step(3, "BK3266 BT receiver + UART control",
        ["Napájení BK3266: <b>VCC 3,3 V</b> z externí AMS1117 LDO, GND společný.",
         "I²S výstupy BK3266: <b>BCK → MUX BT_BCK</b>, <b>LRC → MUX BT_LRC</b>, "
         "<b>SDATA → MUX BT_SDA</b>.",
         "UART2 control: <b>ESP GPIO 17 → BK3266 RX</b>, <b>ESP GPIO 18 ← BK3266 TX</b>, "
         "<b>ESP GPIO 14 → BK3266 nReset</b> (active LOW).",
         "MUX SEL: <b>ESP GPIO 4 → MUX SEL pin</b>. Default LOW = ESP audio, HIGH = BT audio."]))

    story.append(step(4, "I²C OLED + EC11 enkodér",
        ["I²C: <b>GPIO 8 → SDA</b>, <b>GPIO 9 → SCL</b>. OLED napájení 3,3 V (NIKDY 5 V).",
         "Enkodér: A=GPIO 10, B=GPIO 11, SW=GPIO 12, společný kolík + tělo na GND."]))

    story.append(step(5, "Audio cesta DAC → AMP",
        ["Stíněný kabel max. 10 cm: PCM5102A <b>L/R OUT</b> → TPA3110D2 <b>L/R IN</b>. "
         "<b>Stínění svaž jen na DAC straně</b> (jinak ground loop a 50 Hz brum).",
         "<b>Důležité:</b> na XH-A232 modulu nastav <b>GAIN jumpery na 12 dB</b> (zkrať střední pin "
         "ke GND pinu). PCM5102A line-out 2,1 Vrms by jinak při GAIN 20 dB klipoval — "
         "TPA3110 datasheet uvádí max 1 Vrms input.",
         "MUTE: <b>GPIO 21 → AMP MUTE</b>. Bez něj je AMP defaultně muted."]))

    story.append(step(6, "Napájení — power tree",
        ["F1 (1,6 A T) v sérii s L vodičem 230 V před IRM-45-24.",
         "<b>+24 V</b> z IRM-45-24 vede silnějším drátem (≥ 0,75 mm² / 18 AWG) na "
         "<b>VIN/GND</b> svorky TPA3110D2.",
         "<b>Odbočka 24 V</b> do HLK-5M05 (24 V → 5 V).",
         "Z HLK-5M05 výstupu přes <b>ferritový bead BLM31KN601</b> a <b>47 µF kondenzátor</b> "
         "do AMS1117 LDO (VIN 5 V) i do <b>DevKit VIN pin</b>.",
         "AMS1117 výstup 3,3 V napájí: PCM5102A, OLED, BK3266, NX3L4684 MUX."]))

    story.append(step(7, "Reproduktory",
        ["8 Ω reproduktory na <b>SPK L+/-</b> a <b>SPK R+/-</b>. "
         "<b>Nepřipojuj společný GND</b> mezi kanály — výstup TPA3110D2 je BTL.",
         "Doporučená délka kabelu < 2 m (jinak výstupní filtr může oscilovat)."]))

    story.append(step(8, "Vizuální kontrola před prvním zapnutím",
        ["Multimetrem (módu odporu): zkrat <b>+24 V × GND</b>? Musí být vysoký odpor (> 1 kΩ).",
         "Zkrat <b>+5 V × GND</b>? Stejně.",
         "Zkrat <b>+3,3 V × GND</b>? Stejně.",
         "Polarita C1, C2 (1000 µF)?",
         "Pin GAIN jumper na XH-A232 = 12 dB?",
         "USB-C dosažitelný pro flashování?"]))

    story.append(step(9, "První napájení s proudovým omezením",
        ["Pokud máš laboratorní zdroj: nahraď IRM-45-24 zdrojem 24 V s limit 500 mA.",
         "Zapni — odběr by měl být ~150 mA (AMP klid + logika). Pokud > 400 mA, vypni "
         "a zkontroluj zapojení.",
         "Pokud OLED svítí splash a nic nedýmá → vyměň za IRM-45-24."]))

    story.append(step(10, "Flashování firmwaru a první test",
        ["Připoj USB-C k DevKitu, postup viz sekce 6 (firmware bring-up).",
         "V CLI: <code>tone</code> — musíš slyšet 1 kHz tón.",
         "Otáčej enkodérem — hlasitost se mění po 2 % krocích.",
         "Spáruj telefon s <b>SokolAudio</b>, dlouze stiskni enkodér (přepnutí na BT), "
         "spusť přehrávání → musí hrát."]))

    # ---- 4. Schéma zapojení (landscape) ----
    story.append(NextPageTemplate("landscape"))
    story.append(PageBreak())                      # next page = landscape
    story.append(wiring_diagram())
    story.append(NextPageTemplate("portrait"))     # queue portrait for next break

    # ---- 5. Pinout + výpočet napájení ----
    story.append(PageBreak())
    story.append(Paragraph("5. Pinout — kompletní mapa GPIO v1.1", S_H1))

    pin_rows = [
        ["GPIO", "Funkce", "Připojení", "Pozn."],
        ["5",   "I²S BCLK",     "MUX BCK_E → DAC BCK",     "1,4112 MHz @ 44,1 kHz"],
        ["6",   "I²S LRCK",     "MUX LRC_E → DAC LCK",     "44,1 kHz word select"],
        ["7",   "I²S DOUT",     "MUX SDA_E → DAC DIN",     "stereo S16LE"],
        ["15",  "I²S MCLK*",    "PCM5102A SCK",            "Kconfig SOKOL_DAC_MCLK_ENABLE"],
        ["8, 9", "I²C SDA/SCL", "SSD1306",                  "400 kHz"],
        ["10, 11, 12", "Encoder A/B/SW", "EC11",            "PU + IRQ"],
        ["21",  "AMP MUTE",     "TPA3110D2 MUTE",           "active HIGH = ticho"],
        ["13",  "DAC XSMT*",    "PCM5102A XSMT",            "Kconfig"],
        ["<b>4</b>",  "<b>I²S MUX SEL</b>", "<b>NX3L4684 SEL</b>", "<b>v1.1: 0=ESP, 1=BT</b>"],
        ["<b>14</b>", "<b>BK3266 RESET</b>", "<b>BK3266 nReset</b>", "<b>v1.1: active LOW pulse</b>"],
        ["<b>17</b>", "<b>UART2 TX</b>", "<b>BK3266 RX</b>",         "<b>v1.1: AT příkazy 115200</b>"],
        ["<b>18</b>", "<b>UART2 RX</b>", "<b>BK3266 TX</b>",         "<b>v1.1: status events</b>"],
        ["38, 39", "LED status*", "WiFi/BT LED",            "Kconfig"],
    ]
    story.append(small_table(pin_rows, [22 * mm, 30 * mm, 50 * mm, 65 * mm]))
    story.append(Spacer(1, 6))
    story.append(Paragraph("* = volitelný pin (Kconfig). <b>Tučně</b> = přidáno v v1.1.", S_NOTE))

    story.append(Spacer(1, 14))
    story.append(Paragraph("Výpočet napájení — souhrnná tabulka", S_H2))
    pwr = [
        ["Větev", "U", "I typ", "I peak", "Zdroj", "Ztráta"],
        ["TPA3110D2 @ 2×15 W", "24 V", "150 mA", "1,44 A", "IRM-45-24 (1,88 A)", "4,5 W čip"],
        ["Logic 5 V (HLK-5M05 IN)", "24 V", "0,2 A", "0,5 A", "stejný 24 V rail", "0,33 W HLK"],
        ["DevKit ESP32-S3", "5 V", "120 mA", "500 mA", "HLK-5M05", "0,93 W (DevKit AMS)"],
        ["BK3266 BT", "3,3 V", "35 mA", "50 mA", "ext. AMS1117 z 5 V", "<0,1 W"],
        ["PCM5102A DAC", "3,3 V", "20 mA", "30 mA", "ext. AMS1117 z 5 V", "<0,1 W"],
        ["SSD1306 OLED", "3,3 V", "8 mA", "20 mA", "ext. AMS1117 z 5 V", "<0,1 W"],
        ["NX3L4684 MUX", "3,3 V", "<1 mA", "10 mA", "ext. AMS1117 z 5 V", "<0,01 W"],
    ]
    story.append(small_table(pwr, [38*mm, 12*mm, 15*mm, 18*mm, 35*mm, 35*mm]))

    story.append(PageBreak())

    # ---- 6. Firmware bring-up ----
    story.append(Paragraph("6. Firmware — bring-up, CLI, ADF, AVRCP, OTA", S_H1))

    story.append(Paragraph("Toolchain a build", S_H2))
    story.append(Paragraph(
        "ESP-IDF v5.2.x + ESP-ADF v2.6 (musí být exportovaná proměnná <code>ADF_PATH</code>):", S_BODY))
    story.append(Spacer(1, 4))
    story.append(Paragraph(
        "git clone --recursive https://github.com/espressif/esp-adf.git $env:USERPROFILE\\esp\\esp-adf<br/>"
        "git -C $env:USERPROFILE\\esp\\esp-adf checkout v2.6<br/>"
        "$env:ADF_PATH = \"$env:USERPROFILE\\esp\\esp-adf\"<br/>"
        "<br/>"
        "cd smart_amp_proto<br/>"
        "idf.py set-target esp32s3<br/>"
        "idf.py build<br/>"
        "idf.py -p COM5 flash monitor", S_CODE))

    story.append(Paragraph("Sériová konzole — kompletní příkazy", S_H2))
    cli = [
        ["Příkaz", "Popis"],
        ["help",                     "Vypíše všechny příkazy"],
        ["tone [Hz] [ms] [amp%]",    "Sinusový testovací tón na DAC (default 1 kHz)"],
        ["volume [0..100]",          "Software volume (jen pro Wi-Fi UDP zdroj)"],
        ["source &lt;wifi|bt|none&gt;", "Přepne aktivní zdroj (přepne MUX)"],
        ["wifi set &lt;ssid&gt; &lt;pass&gt;", "Uloží Wi-Fi credentials, pak <code>reboot</code>"],
        ["info",                     "Stav: zdroj, volume, Wi-Fi IP, BT, packet loss, heap"],
        ["heap",                     "Vypíše internal + PSRAM haldu"],
        ["<b>bt &lt;play|pause|next|prev|reset|status&gt;</b>",
         "<b>v1.1:</b> AVRCP řízení vzdálené strany přes BK3266 UART"],
        ["<b>ota &lt;url&gt;</b>",
         "<b>v1.1:</b> spustí HTTPS OTA z URL (https:// nebo http:// jen lokální IP)"],
        ["<b>ota mark-valid</b>",
         "<b>v1.1:</b> potvrdí aktuální boot, zruší rollback (auto za 30 s běhu)"],
        ["reboot",                   "Softwarový reset"],
    ]
    story.append(small_table(cli, [55 * mm, 110 * mm]))

    story.append(PageBreak())

    # ---- 7. ADF Pipeline + AVRCP + OTA ----
    story.append(Paragraph("7. ADF audio pipeline pro FLAC + Opus", S_H1))
    story.append(Paragraph(
        "Pipeline (<code>codecs/stream_decoder.c</code>):", S_BODY))
    story.append(Spacer(1, 6))
    story.append(Paragraph(
        "  UDP rx → <b>raw_in (writer)</b> → <b>flac_decoder | opus_decoder</b> → "
        "<b>raw_out (reader)</b> → pump task → i2s_dma → DAC", S_CODE))
    story.append(Spacer(1, 6))
    story.append(Paragraph(
        "Decoder elementy jsou z <code>esp-adf-libs</code>. Pump task na Core 1 (priorita 9) "
        "čte PCM z <code>raw_out</code> a tlačí ho do existujícího <code>i2s_dma_push()</code> "
        "ringbufferu — tím zachová stejnou volume_ctrl + DMA cestu jako pro RAW PCM. "
        "Přepínání kodeků: každé volání <code>stream_decoder_begin(codec, rate, ch)</code> "
        "tear-downuje předchozí pipeline (čisté unregister) a postaví novou.", S_BODY))

    story.append(Paragraph("Latence + buffery", S_H2))
    adf_lat = [
        ["Stage", "Buffer", "Latence @ 44,1 kHz"],
        ["raw_in ringbuffer", "8 KB", "~46 ms (komp. data)"],
        ["FLAC decoder pacing", "internal frame", "~10 ms"],
        ["raw_out ringbuffer", "8 KB", "~46 ms PCM"],
        ["Pump task buffer", "2 KB", "~12 ms"],
        ["i2s_dma ringbuf (PSRAM)", "2 MB", "~5,8 s max (buffer pro reconnect)"],
    ]
    story.append(small_table(adf_lat, [55*mm, 35*mm, 50*mm]))

    story.append(Spacer(1, 14))
    story.append(Paragraph("8. AVRCP routing (Bluetooth ovládání)", S_H1))
    story.append(Paragraph(
        "BK3266 modul přijímá AVRCP příkazy z telefonu a posílá status zprávy "
        "přes UART2 (115200 baud). <code>network/bt_a2dp_sink.c</code> obsahuje "
        "line-by-line parser, který tyto zprávy mapuje na <code>bt_avrcp_event_t</code> "
        "a posílá registrovanému callbacku v <code>source_manager.c</code>.", S_BODY))

    avrcp = [
        ["Vstup z telefonu", "BK3266 UART → ESP", "Akce v firmware"],
        ["telefon spáruje", "+STAT:CONNECTED", "auto-switch na BT zdroj (pokud SRC_NONE)"],
        ["telefon přehrává", "+STAT:PLAYING", "switch na BT, un-mute"],
        ["telefon pauzne", "+STAT:PAUSED", "log only (AMP zůstane unmuted)"],
        ["telefon next", "+STAT:NEXT", "log only"],
        ["telefon disconnect", "+STAT:DISCONNECTED", "switch na SRC_NONE"],
        ["", "", ""],
        ["Vstup z enkodéru", "Akce", "ESP → BK3266 UART"],
        ["short press (BT mode)", "AMP toggle + AVRCP play/pause", "AT+CC nebo AT+CD"],
        ["rotate CW (BT mode)", "Volume up", "AT+VOL+"],
        ["rotate CCW (BT mode)", "Volume down", "AT+VOL-"],
        ["long press", "Cycle source (WiFi → BT → none)", "—"],
    ]
    story.append(small_table(avrcp, [50 * mm, 50 * mm, 65 * mm]))

    story.append(PageBreak())

    story.append(Paragraph("9. OTA firmware update (HTTPS + rollback)", S_H1))
    story.append(Paragraph(
        "Implementace v <code>network/ota_updater.c</code>. Používá "
        "<code>esp_https_ota</code> s mozilla CA bundle "
        "(<code>CONFIG_MBEDTLS_CERTIFICATE_BUNDLE</code>) pro TLS validaci. "
        "Partition layout (viz <code>partitions.csv</code>): factory + ota_0 + ota_1, "
        "každý 2 MB. Bootloader app rollback je zapnutý: pokud nový obraz po 30 s "
        "běhu nezavolá <code>ota_updater_mark_valid()</code>, na příštím restartu "
        "bootloader vrátí předchozí.", S_BODY))

    story.append(Paragraph("Použití z CLI", S_H2))
    story.append(Paragraph(
        "ota https://ota.example.com/sa32/v1.1.bin<br/>"
        "  # nebo lokální HTTP server pro vývoj:<br/>"
        "ota http://192.168.1.100:8000/sa32-fw.bin<br/>"
        "<br/>"
        "  # ručně potvrdit boot (jinak auto za 30 s):<br/>"
        "ota mark-valid", S_CODE))

    story.append(Paragraph("OTA průběh", S_H2))
    for it in [
        "1. CLI ověří URL (https:// vždy, http:// jen pro 192.168/10/172.16-31).",
        "2. Background task: <code>esp_https_ota_begin()</code> + ověření header (project_name, version).",
        "3. Stahování po 16 KB chunks; AMP po dobu OTA v MUTE.",
        "4. <code>esp_https_ota_finish()</code> verifikuje SHA, označí nový slot jako bootable.",
        "5. <code>esp_restart()</code>; bootloader nabootuje nový obraz s flag PENDING_VERIFY.",
        "6. Po 30 s běhu volá main: <code>esp_ota_mark_app_valid_cancel_rollback()</code>.",
        "7. Pokud panic dříve, restart → bootloader rollback na předchozí slot.",
    ]:
        story.append(li(it))

    story.append(Paragraph("Bezpečnostní opatření", S_H2))
    for it in [
        "TLS s ověřenou cert chain (mozilla CA bundle).",
        "HTTP povolené pouze pro RFC 1918 lokální adresy (192.168.x.x, 10.x, 172.16-31.x).",
        "Image SHA verifikace v <code>esp_https_ota_finish()</code>.",
        "App rollback: dva selhané boot pokusy = návrat na minulý obraz.",
        "AMP v MUTE během stahování — vyloučí audio glitch při OTA.",
    ]:
        story.append(li(it))

    story.append(PageBreak())

    # ---- 10. Streamování zvuku ----
    story.append(Paragraph("10. Streamování zvuku", S_H1))

    story.append(Paragraph("Wi-Fi: raw PCM přes UDP", S_H2))
    story.append(Paragraph(
        "Port <b>5005</b> (Kconfig SOKOL_UDP_PORT). Hlavička 11 B (LE):", S_BODY))
    story.append(Paragraph(
        "0..3   magic     'S' 'O' 'K' 'A'<br/>"
        "4      codec     0=PCM, 1=FLAC, 2=Opus<br/>"
        "5..7   rate      sample rate (24-bit)<br/>"
        "8      channels  počet kanálů<br/>"
        "9..10  length    délka payloadu<br/>"
        "11..   payload   PCM nebo komprimovaná data", S_CODE))
    story.append(Spacer(1, 4))
    story.append(Paragraph("FLAC a Opus jsou dekódované přes ADF audio_pipeline (sekce 7).", S_NOTE))

    story.append(Paragraph("Bluetooth A2DP / SBC (přes BK3266)", S_H2))
    story.append(Paragraph(
        "Telefon vidí zařízení jako <b>SokolAudio</b>. Spárování + přehrávání. "
        "Audio teče: telefon → BK3266 (SBC dekód) → I²S → MUX → DAC → AMP. "
        "ESP32 do tohoto streamu <i>nezasahuje</i> — jen přepíná MUX a routuje "
        "AVRCP příkazy z UART.", S_BODY))

    story.append(Spacer(1, 14))

    # ---- 11. Validační report ----
    story.append(Paragraph("11. Validační report HW", S_H1))
    story.append(Paragraph(
        "Patnáct kontrol pokrývajících PSU rezervu, termální budget, voltage matching, "
        "logické úrovně, GPIO konflikty a OTA bezpečnost. <b>OK</b> = ověřeno datasheety; "
        "<b>BORDERLINE</b> = na hraně specifikace; <b>ACTION</b> = vyžaduje uživatelský "
        "krok při sestavení; <b>DOC</b> = řešeno dokumentací, ne HW.", S_BODY))
    story.append(Spacer(1, 6))

    val = [
        ["#", "Kontrola", "Spec / norma", "Výsledek", "Pozn."],
        ["1", "PSU nadproudová rezerva", "I_max ≥ 1,3 × I_peak", "OK", "1,88 / 1,44 = 130 %"],
        ["2", "5 V buck výkonová rezerva", "P_max ≥ 1,5 × P_avg", "OK", "5 W / 2,8 W = 178 %"],
        ["3", "AMS1117 termální (DevKit)", "P_diss < 1 W bez heatsinku", "BORDERLINE",
         "0,93 W peak — odlehčit ext. LDO"],
        ["4", "I²S BCLK přes MUX", "f_max NX3L4684 > 1,4 MHz", "OK", "NX3L4684 propustnost 100+ MHz"],
        ["5", "Logické úrovně I²S", "V_OH ≥ 2,0 V (V_IH PCM5102A)", "OK", "vše 3,3 V CMOS"],
        ["6", "DAC → AMP voltage match", "V_in ≤ 1 Vrms (GAIN 20 dB)", "ACTION",
         "Nastavit GAIN jumper na 12 dB"],
        ["7", "Pojistka primár", "1,6 A T pro 45 W zdroj", "OK", "F1 v sérii s L 230 V"],
        ["8", "BK3266 baud rate", "115200 default firmware", "OK", "většina klonů"],
        ["9", "Partition table OTA", "2× ota_X (2 MB) + factory", "OK", "partitions.csv beze změny"],
        ["10","Disable BR/EDR (S3)", "ESP_BT_ENABLED=n", "OK", "úspora ~80 KB internal RAM"],
        ["11","GPIO konflikty", "vyhnout se PSRAM 35-37, flash 26-32", "OK",
         "MUX_SEL 4, BK_RST 14, UART2 17/18"],
        ["12","Audio cable shield", "ground only at DAC end", "DOC", "v PDF, vizuální kontrola"],
        ["13","Star ground topologie", "single junction at PSU GND", "DOC", "manuálně instaluje"],
        ["14","OTA rollback", "BOOTLOADER_APP_ROLLBACK_ENABLE=y", "OK", "auto-validate po 30 s"],
        ["15","HTTPS cert verify", "MBEDTLS_CERTIFICATE_BUNDLE=y", "OK", "mozilla CA bundle"],
    ]
    story.append(small_table(val, [8*mm, 38*mm, 42*mm, 22*mm, 55*mm], status_col=3))

    story.append(PageBreak())

    # ---- 12. Řešení potíží ----
    story.append(Paragraph("12. Řešení potíží", S_H1))
    tr = [
        ["Symptom", "Pravděpodobná příčina", "Řešení"],
        ["Restart pri zapnutí AMP", "PSU poddimenzovaný (Phase 1 IRM-20-24 měl 0,83 A)",
         "Použij IRM-45-24 (1,88 A) podle BOM v1.1"],
        ["Zesilovač 'mrtvý', OLED nesvítí", "5 V nebo 3,3 V chybí na DevKitu",
         "Změř pin <code>5V</code> na DevKitu — musí být ~5,0 V z HLK-5M05. Pak <code>3V3</code> = 3,3 V."],
        ["OLED svítí, audio nehraje", "AMP MUTE, špatný I²S sled, nebo MUX na špatné straně",
         "<code>tone</code> v CLI; změř pin GPIO 4 (MUX SEL = 0 pro Wi-Fi); GPIO 21 = LOW"],
        ["BT se nepáří se SokolAudio", "BK3266 nemá napájení, nebo špatný UART link",
         "Zkontroluj 3,3 V na BK3266; <code>bt status</code> v CLI; v logu hledej '+STAT:'"],
        ["BT zvuk hraje, hlasitost se nemění z enkodéru", "BK3266 firmware ignoruje AT+VOL",
         "Některé klony nemají volume přes UART. Volume nastav na telefonu."],
        ["Klipping při hlasitějších kanálech", "GAIN jumper na XH-A232 stále 20 dB",
         "Přepoj jumper na 12 dB pozici (AMP datasheet, sekce 7.4)"],
        ["Brum 50 Hz", "Ground loop mezi DAC a AMP",
         "Stínění audio kabelu jen na DAC straně. Star ground na PSU."],
        ["Praskání při Wi-Fi a BT současně", "—",
         "v1.1 je BT externí, koexistence není problém. Pokud praskání: zkontroluj HLK-5M05 výstup."],
        ["OTA selže s 'cert verify failed'", "URL používá self-signed nebo nedostupný CA",
         "Použij Let's Encrypt cert, nebo lokální HTTP (192.168.x.x)"],
        ["OTA OK ale po restartu se vrátí stará verze", "Boot rollback po 2× selhání",
         "Hledej panic v logu; po debug volej <code>ota mark-valid</code> v 30 s okně"],
        ["FLAC stream se nepřehrává", "ADF pipeline nezakládá nebo špatný codec id",
         "Hledej 'ADF pipeline up' v logu; potvrď codec=1 v UDP hlavičce"],
    ]
    story.append(small_table(tr, [42 * mm, 55 * mm, 75 * mm]))

    story.append(PageBreak())

    # ---- 13. Roadmap ----
    story.append(Paragraph("13. Roadmap (po v1.1)", S_H1))
    for it in [
        "v1.2: Custom PCB (DPS) místo proto-board sestavy.",
        "v1.2: BLE companion app pro vzdálenou konfiguraci (replace UART CLI).",
        "v1.3: Externí I²S vstup (HDMI ARC, Toslink) → další větev MUX.",
        "v1.3: Multi-page OLED menu (SSID picker, EQ, BT pair manager).",
        "v1.3: Dual-channel DSP (loudness, crossover) na ESP32-S3 PSRAM.",
        "v2.0: TAS5825M místo TPA3110D2 (2× 38 W, integrovaný DSP, vyžaduje IRM-90-24).",
    ]:
        story.append(li(it))

    doc.build(story)
    print(f"Wrote {OUT}")

if __name__ == "__main__":
    build()

/* External BT A2DP receiver driver — BK3266 / BLK-MD-SPK style I²S modul.
 *
 * Architektura v1.1:
 *   ESP32-S3 ──UART2──► BK3266 (BT 5.0 BR/EDR + A2DP/AVRCP/SBC)
 *                          │
 *                          └──► I²S out (BCK/LRCK/SDATA) ──► hardware MUX ──► PCM5102A
 *
 * UART AT protokol (typický pro BK3266 firmware revision 4.x):
 *   ESP → BK:
 *     AT+RESET\r\n         softwarový reset
 *     AT+NAME=SokolAudio\r\n  nastavit zobrazované jméno
 *     AT+DISCON\r\n         odpojit aktuální klient
 *     AT+VOL=15\r\n         absolutní volume (0–15) na vzdálené straně přes AVRCP
 *     AT+CC\r\n             play
 *     AT+CD\r\n             pause
 *     AT+CE\r\n             next track
 *     AT+CF\r\n             previous track
 *
 *   BK → ESP (asynchronní status):
 *     +STAT:CONNECTED       spárováno + připojeno
 *     +STAT:DISCONNECTED    odpojeno
 *     +STAT:PLAYING         přehrávání zahájeno (= AVRCP play notif)
 *     +STAT:PAUSED          přehrávání pauznuto
 *     +STAT:NEXT / PREV     uživatel pohnul stopou na dálkovém ovladači
 *
 * Pokud máš jiný BK3266 firmware (např. SPP-only), uprav SEND_AT() makro
 * a parser_line() — protokol je u různých dodavatelů mírně odlišný.
 *
 * Bezpečnost: UART parser pracuje s pevně omezeným bufferem (128 B/řádek).
 * Vstup z BT modulu je důvěryhodný (přímý spoj na DPS), takže žádná další
 * sanitizace není nutná, ale buffer overflow ošetřen.
 */

#include "bt_a2dp_sink.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdatomic.h>
#include <stdio.h>

static const char *TAG = "bt_bk3266";

/* --- Pin & UART config (sladěno s docs/SA32_navod_sestaveni.pdf) ----------- */
#define BK_UART_NUM      UART_NUM_2
#define BK_UART_TX_GPIO  17
#define BK_UART_RX_GPIO  18
#define BK_RESET_GPIO    14          /* active LOW; nepoužitý → ponech HIGH */
#define BK_UART_BAUD     115200      /* default BK3266 baud */
#define BK_LINE_BUF      128
#define BK_RX_QUEUE_LEN  16

static atomic_bool   s_enabled    = ATOMIC_VAR_INIT(false);
static atomic_bool   s_connected  = ATOMIC_VAR_INIT(false);
static bt_avrcp_cb_t s_cb         = NULL;
static void         *s_cb_ctx     = NULL;
static TaskHandle_t  s_rx_task    = NULL;

/* Helper: jeden AT řádek + CRLF, blokuje do dokončení TX FIFO. */
static esp_err_t bk_send_at(const char *line)
{
    if (!line) return ESP_ERR_INVALID_ARG;
    int n = uart_write_bytes(BK_UART_NUM, line, strlen(line));
    if (n < 0) return ESP_FAIL;
    uart_write_bytes(BK_UART_NUM, "\r\n", 2);
    uart_wait_tx_done(BK_UART_NUM, pdMS_TO_TICKS(100));
    ESP_LOGD(TAG, "TX> %s", line);
    return ESP_OK;
}

/* Mapování AVRCP enum → AT příkaz. Drží se BK3266 default. */
static const char *avrcp_to_at(bt_avrcp_event_t cmd)
{
    switch (cmd) {
        case BT_AVRCP_PLAY:     return "AT+CC";
        case BT_AVRCP_PAUSE:    return "AT+CD";
        case BT_AVRCP_NEXT:     return "AT+CE";
        case BT_AVRCP_PREV:     return "AT+CF";
        case BT_AVRCP_VOL_UP:   return "AT+VOL+";
        case BT_AVRCP_VOL_DOWN: return "AT+VOL-";
        default:                return NULL;
    }
}

/* Parser jednoho UART řádku (bez CR/LF). */
static void parser_line(const char *line)
{
    if (!line || !*line) return;
    ESP_LOGD(TAG, "RX< %s", line);

    /* Asynchronní status notifikace */
    if (!strncmp(line, "+STAT:", 6)) {
        const char *st = line + 6;
        if      (!strcmp(st, "CONNECTED"))    { atomic_store(&s_connected, true);  if (s_cb) s_cb(BT_AVRCP_CONNECTED, s_cb_ctx); }
        else if (!strcmp(st, "DISCONNECTED")) { atomic_store(&s_connected, false); if (s_cb) s_cb(BT_AVRCP_DISCONNECTED, s_cb_ctx); }
        else if (!strcmp(st, "PLAYING"))      { if (s_cb) s_cb(BT_AVRCP_PLAY,  s_cb_ctx); }
        else if (!strcmp(st, "PAUSED"))       { if (s_cb) s_cb(BT_AVRCP_PAUSE, s_cb_ctx); }
        else if (!strcmp(st, "NEXT"))         { if (s_cb) s_cb(BT_AVRCP_NEXT,  s_cb_ctx); }
        else if (!strcmp(st, "PREV"))         { if (s_cb) s_cb(BT_AVRCP_PREV,  s_cb_ctx); }
        else ESP_LOGI(TAG, "stat: %s", st);
    } else if (!strncmp(line, "OK", 2) || !strncmp(line, "ERROR", 5)) {
        /* normalní AT odpověď, ignoruj */
    } else {
        /* pomocná metadata (TRACK info, název, atd.) — log jen */
        ESP_LOGI(TAG, "%s", line);
    }
}

/* Read UART, splice do řádků, volej parser. Pevný buffer, bez heap alokace. */
static void rx_task(void *arg)
{
    (void)arg;
    char line[BK_LINE_BUF];
    size_t len = 0;
    uint8_t b;

    while (1) {
        int n = uart_read_bytes(BK_UART_NUM, &b, 1, pdMS_TO_TICKS(500));
        if (n <= 0) continue;
        if (b == '\r') continue;
        if (b == '\n' || len >= BK_LINE_BUF - 1) {
            line[len] = '\0';
            parser_line(line);
            len = 0;
        } else {
            line[len++] = (char)b;
        }
    }
}

/* --- Public API ----------------------------------------------------------- */

esp_err_t bt_a2dp_sink_init(void)
{
    /* RESET pin: HIGH = enable (BK3266 nReset). */
    gpio_config_t rst_io = {
        .pin_bit_mask = 1ULL << BK_RESET_GPIO,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&rst_io), TAG, "rst gpio");
    gpio_set_level(BK_RESET_GPIO, 0);             /* drž v resetu */

    /* UART2 setup */
    uart_config_t uart_cfg = {
        .baud_rate = BK_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(BK_UART_NUM, 1024, 1024,
                                            BK_RX_QUEUE_LEN, NULL, 0),
                        TAG, "uart install");
    ESP_RETURN_ON_ERROR(uart_param_config(BK_UART_NUM, &uart_cfg), TAG, "uart cfg");
    ESP_RETURN_ON_ERROR(uart_set_pin(BK_UART_NUM, BK_UART_TX_GPIO, BK_UART_RX_GPIO,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart pin");

    /* RX task */
    BaseType_t ok = xTaskCreatePinnedToCore(rx_task, "bk3266_rx", 3072, NULL, 7, &s_rx_task, 0);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_FAIL, TAG, "rx task");

    /* Pust BK3266 z resetu, dej mu 200 ms na boot, pak nastav jméno. */
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(BK_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    bk_send_at("AT+NAME=SokolAudio");

    ESP_LOGI(TAG, "BK3266 controller up (UART2 @%d baud, TX=%d RX=%d, RST=%d)",
             BK_UART_BAUD, BK_UART_TX_GPIO, BK_UART_RX_GPIO, BK_RESET_GPIO);
    return ESP_OK;
}

void bt_a2dp_sink_enable(bool enable)
{
    atomic_store(&s_enabled, enable);
    /* "enable" v této architektuře jen logicky povolí, že source_manager očekává
     * BT zvuk. HW MUX se přepíná zvlášť přes i2s_mux_select(). BK3266 zůstává
     * spárovaný i v "disabled" stavu — pouze se jeho I²S výstup nepouští do DAC. */
    ESP_LOGI(TAG, "%s (BK3266 zůstává napájen, jen MUX odpojí)", enable ? "enabled" : "disabled");
}

bool bt_a2dp_sink_is_connected(void) { return atomic_load(&s_connected); }

void bt_a2dp_sink_on_avrcp(bt_avrcp_cb_t cb, void *ctx)
{
    s_cb = cb;
    s_cb_ctx = ctx;
}

esp_err_t bt_a2dp_sink_send(bt_avrcp_event_t cmd)
{
    const char *at = avrcp_to_at(cmd);
    if (!at) return ESP_ERR_INVALID_ARG;
    return bk_send_at(at);
}

esp_err_t bt_a2dp_sink_reset(void)
{
    /* HW reset: pulse RESET pin LOW. */
    gpio_set_level(BK_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(BK_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    bk_send_at("AT+NAME=SokolAudio");
    atomic_store(&s_connected, false);
    ESP_LOGW(TAG, "BK3266 hardware reset");
    return ESP_OK;
}

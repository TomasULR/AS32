#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "drivers/i2s_pcm5102a.h"
#include "drivers/amp_control.h"
#include "drivers/oled_ssd1306.h"
#include "drivers/rotary_ec11.h"
#include "drivers/i2s_mux.h"

#include "audio/i2s_dma.h"
#include "audio/volume_ctrl.h"
#include "audio/source_manager.h"

#include "codecs/stream_decoder.h"

#include "network/wifi_sta.h"
#include "network/udp_receiver.h"
#include "network/bt_a2dp_sink.h"
#include "network/ota_updater.h"

#include "storage/nvs_config.h"
#include "utils/serial_cli.h"
#include "utils/heap_debug.h"

static const char *TAG = "main";

#define MUX_SEL_GPIO   GPIO_NUM_4    /* I²S MUX select: 0=ESP, 1=BT */
#define BOOT_VALIDATE_DELAY_MS  30000  /* po OTA potvrď za 30 s běhu */

static void ui_task(void *arg)
{
    (void)arg;
    QueueHandle_t q = rotary_ec11_queue();
    rotary_event_t ev;
    int vol = volume_ctrl_get_percent();
    const char *src = source_manager_name(source_manager_get());
    oled_render_status("SokolAudio", src, vol);

    while (1) {
        if (xQueueReceive(q, &ev, pdMS_TO_TICKS(500)) == pdTRUE) {
            switch (ev.type) {
                case ROTARY_EVT_ROTATE_CW:
                    vol = volume_ctrl_get_percent() + 2;
                    if (vol > 100) vol = 100;
                    volume_ctrl_set_percent(vol);
                    nvs_cfg_set_volume(vol);
                    /* Pokud aktivní zdroj = BT, posli AVRCP volume up vzdálené straně. */
                    if (source_manager_get() == SRC_BT_A2DP) bt_a2dp_sink_send(BT_AVRCP_VOL_UP);
                    break;
                case ROTARY_EVT_ROTATE_CCW:
                    vol = volume_ctrl_get_percent() - 2;
                    if (vol < 0) vol = 0;
                    volume_ctrl_set_percent(vol);
                    nvs_cfg_set_volume(vol);
                    if (source_manager_get() == SRC_BT_A2DP) bt_a2dp_sink_send(BT_AVRCP_VOL_DOWN);
                    break;
                case ROTARY_EVT_PRESS:
                    /* Krátký stisk: BT play/pause | jinak amp mute toggle */
                    if (source_manager_get() == SRC_BT_A2DP) {
                        /* heuristika: pokud je amp un-muted, pošleme PAUSE, jinak PLAY */
                        bt_a2dp_sink_send(amp_control_is_muted() ? BT_AVRCP_PLAY : BT_AVRCP_PAUSE);
                    }
                    amp_control_set_mute(!amp_control_is_muted());
                    break;
                case ROTARY_EVT_LONG_PRESS:
                    source_manager_cycle();
                    break;
            }
        }
        src = source_manager_name(source_manager_get());
        oled_render_status("SokolAudio", src, volume_ctrl_get_percent());
    }
}

/* OTA validace: 30 s po bootu bez panicu = označ obraz jako valid (zruší rollback) */
static void ota_validate_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(BOOT_VALIDATE_DELAY_MS));
    ota_updater_mark_valid();
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== SokolAudio Smart Amp v1.1 boot ===");

    /* 1. Persistent config first — everything else may read it. */
    ESP_ERROR_CHECK(nvs_cfg_init());

    /* 2. AMP up muted before any audio pin is configured. */
    ESP_ERROR_CHECK(amp_control_init());

    /* 3. I²C bus + OLED splash. */
    ESP_ERROR_CHECK(oled_init());
    oled_draw_text(0, 0, "SokolAudio");
    oled_draw_text(0, 2, "booting v1.1...");
    oled_flush();

    /* 4. Rotary encoder. */
    ESP_ERROR_CHECK(rotary_ec11_init());

    /* 5. I²S MUX (přepínač mezi ESP I²S a BK3266 I²S). Default = ESP. */
    ESP_ERROR_CHECK(i2s_mux_init(MUX_SEL_GPIO));

    /* 6. ESP I²S master k PCM5102A + DMA pump + volume. */
    ESP_ERROR_CHECK(i2s_pcm5102a_init(I2S_PCM_RATE_44100));
    i2s_dma_fmt_t fmt = { .sample_rate = 44100, .channels = 2, .bits_per_sample = 16 };
    ESP_ERROR_CHECK(i2s_dma_init(&fmt));
    ESP_ERROR_CHECK(volume_ctrl_init());
    volume_ctrl_set_percent(nvs_cfg_get_volume());

    /* 7. ADF stream decoder (RAW PCM vždy + FLAC/Opus přes ADF pipeline). */
    ESP_ERROR_CHECK(stream_decoder_init());

    /* 8. Síť: Wi-Fi STA + UDP receiver. */
    ESP_ERROR_CHECK(wifi_sta_init());
    ESP_ERROR_CHECK(udp_receiver_init());

    /* 9. Externí BT receiver (BK3266) přes UART2 + AVRCP routing. */
    ESP_ERROR_CHECK(bt_a2dp_sink_init());

    /* 10. Source manager (registruje AVRCP callback z BK3266). */
    ESP_ERROR_CHECK(source_manager_init());

    /* 11. UI task on Core 0. */
    xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 6, NULL, 0);

    /* 12. CLI + heap monitor + OTA validation. */
    ESP_ERROR_CHECK(serial_cli_start());
    ESP_ERROR_CHECK(heap_debug_start(30000));
    ESP_ERROR_CHECK(ota_updater_init());
    xTaskCreatePinnedToCore(ota_validate_task, "ota_val", 2048, NULL, 1, NULL, 0);

    /* 13. Vše nahoře — drop boot mute. */
    amp_control_set_mute(false);
    ESP_LOGI(TAG, "boot complete (v1.1: HW BT MUX + ADF + OTA), amp unmuted");
}

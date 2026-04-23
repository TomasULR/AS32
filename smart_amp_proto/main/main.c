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

#include "audio/i2s_dma.h"
#include "audio/volume_ctrl.h"
#include "audio/source_manager.h"

#include "codecs/flac_decoder.h"
#include "codecs/aac_sbc_decoder.h"

#include "network/wifi_sta.h"
#include "network/udp_receiver.h"
#include "network/bt_a2dp_sink.h"

#include "storage/nvs_config.h"
#include "utils/serial_cli.h"
#include "utils/heap_debug.h"

static const char *TAG = "main";

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
                    break;
                case ROTARY_EVT_ROTATE_CCW:
                    vol = volume_ctrl_get_percent() - 2;
                    if (vol < 0) vol = 0;
                    volume_ctrl_set_percent(vol);
                    nvs_cfg_set_volume(vol);
                    break;
                case ROTARY_EVT_PRESS:
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

void app_main(void)
{
    ESP_LOGI(TAG, "=== SokolAudio Smart Amp boot ===");

    /* 1. Persistent config first — everything else may read it. */
    ESP_ERROR_CHECK(nvs_cfg_init());

    /* 2. Bring amp up in the muted state before touching audio pins. */
    ESP_ERROR_CHECK(amp_control_init());

    /* 3. I2C bus + OLED splash. */
    ESP_ERROR_CHECK(oled_init());
    oled_draw_text(0, 0, "SokolAudio");
    oled_draw_text(0, 2, "booting...");
    oled_flush();

    /* 4. Rotary encoder. */
    ESP_ERROR_CHECK(rotary_ec11_init());

    /* 5. I2S master to PCM5102A, then the DMA/ringbuffer pump. */
    ESP_ERROR_CHECK(i2s_pcm5102a_init(I2S_PCM_RATE_44100));
    i2s_dma_fmt_t fmt = { .sample_rate = 44100, .channels = 2, .bits_per_sample = 16 };
    ESP_ERROR_CHECK(i2s_dma_init(&fmt));
    ESP_ERROR_CHECK(volume_ctrl_init());
    volume_ctrl_set_percent(nvs_cfg_get_volume());

    /* 6. Decoders (raw PCM always on; FLAC/Opus deferred to ADF pipeline). */
    ESP_ERROR_CHECK(stream_decoder_init());

    /* 7. Network stack: WiFi + BT. Both can coexist on ESP32-S3. */
    ESP_ERROR_CHECK(wifi_sta_init());
    ESP_ERROR_CHECK(udp_receiver_init());
    ESP_ERROR_CHECK(bt_a2dp_sink_init());

    /* 8. Source manager picks the boot source and calls udp/bt enablers. */
    ESP_ERROR_CHECK(source_manager_init());

    /* 9. UI task on Core 0. */
    xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 6, NULL, 0);

    /* 10. Serial CLI + periodic heap logging. */
    ESP_ERROR_CHECK(serial_cli_start());
    ESP_ERROR_CHECK(heap_debug_start(30000));

    /* 11. Everything is up — drop the boot mute. */
    amp_control_set_mute(false);
    ESP_LOGI(TAG, "boot complete, amp unmuted");
}

#include "i2s_pcm5102a.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Pin map — mirrors README wiring table. */
#define I2S_BCLK_GPIO   GPIO_NUM_5
#define I2S_LRCK_GPIO   GPIO_NUM_6
#define I2S_DOUT_GPIO   GPIO_NUM_7
#define I2S_MCLK_GPIO   GPIO_NUM_15

static const char *TAG = "i2s_pcm";

static i2s_chan_handle_t s_tx_chan = NULL;
static SemaphoreHandle_t s_cfg_lock = NULL;
static i2s_pcm_rate_t s_rate = I2S_PCM_RATE_44100;

static esp_err_t apply_clock(i2s_pcm_rate_t rate)
{
    i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)rate);
#if CONFIG_SOKOL_DAC_MCLK_ENABLE
    clk.mclk_multiple = I2S_MCLK_MULTIPLE_256;
#endif
    return i2s_channel_reconfig_std_clock(s_tx_chan, &clk);
}

esp_err_t i2s_pcm5102a_init(i2s_pcm_rate_t rate)
{
    if (s_tx_chan) {
        return ESP_ERR_INVALID_STATE;
    }

    s_cfg_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_cfg_lock, ESP_ERR_NO_MEM, TAG, "mutex alloc");

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 512;
    chan_cfg.auto_clear = true;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL), TAG, "new_channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk =
#if CONFIG_SOKOL_DAC_MCLK_ENABLE
                I2S_MCLK_GPIO,
#else
                I2S_GPIO_UNUSED,
#endif
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_LRCK_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0 },
        },
    };
#if CONFIG_SOKOL_DAC_MCLK_ENABLE
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
#endif

    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx_chan, &std_cfg), TAG, "init_std");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx_chan), TAG, "enable");

    s_rate = rate;
    ESP_LOGI(TAG, "I2S TX up: BCLK=%d LRCK=%d DOUT=%d MCLK=%s rate=%d",
             I2S_BCLK_GPIO, I2S_LRCK_GPIO, I2S_DOUT_GPIO,
#if CONFIG_SOKOL_DAC_MCLK_ENABLE
             "GPIO15",
#else
             "off",
#endif
             (int)rate);
    return ESP_OK;
}

esp_err_t i2s_pcm5102a_set_rate(i2s_pcm_rate_t rate)
{
    if (!s_tx_chan) return ESP_ERR_INVALID_STATE;
    if (rate == s_rate) return ESP_OK;

    xSemaphoreTake(s_cfg_lock, portMAX_DELAY);
    esp_err_t err = i2s_channel_disable(s_tx_chan);
    if (err == ESP_OK) err = apply_clock(rate);
    if (err == ESP_OK) err = i2s_channel_enable(s_tx_chan);
    if (err == ESP_OK) {
        s_rate = rate;
        ESP_LOGI(TAG, "Rate -> %d Hz", (int)rate);
    } else {
        ESP_LOGE(TAG, "Rate switch failed: %s", esp_err_to_name(err));
    }
    xSemaphoreGive(s_cfg_lock);
    return err;
}

esp_err_t i2s_pcm5102a_write(const void *buf, size_t bytes, size_t *written, uint32_t timeout_ms)
{
    if (!s_tx_chan) return ESP_ERR_INVALID_STATE;
    return i2s_channel_write(s_tx_chan, buf, bytes, written, pdMS_TO_TICKS(timeout_ms));
}

i2s_chan_handle_t i2s_pcm5102a_tx_handle(void) { return s_tx_chan; }

esp_err_t i2s_pcm5102a_deinit(void)
{
    if (!s_tx_chan) return ESP_OK;
    i2s_channel_disable(s_tx_chan);
    i2s_del_channel(s_tx_chan);
    s_tx_chan = NULL;
    vSemaphoreDelete(s_cfg_lock);
    s_cfg_lock = NULL;
    return ESP_OK;
}

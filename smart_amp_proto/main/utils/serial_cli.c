#include "serial_cli.h"
#include "i2s_dma.h"
#include "volume_ctrl.h"
#include "source_manager.h"
#include "wifi_sta.h"
#include "nvs_config.h"
#include "amp_control.h"
#include "heap_debug.h"
#include "bt_a2dp_sink.h"
#include "udp_receiver.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"
#include "argtable3/argtable3.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "cli";

/* --- tone <hz> [ms] [amp%] ------------------------------------------------ */
static struct {
    struct arg_int *hz;
    struct arg_int *ms;
    struct arg_int *amp;
    struct arg_end *end;
} tone_args;

static int cmd_tone(int argc, char **argv)
{
    int nerr = arg_parse(argc, argv, (void **)&tone_args);
    if (nerr) { arg_print_errors(stderr, tone_args.end, argv[0]); return 1; }
    int hz  = tone_args.hz->count ? tone_args.hz->ival[0] : 1000;
    int ms  = tone_args.ms->count ? tone_args.ms->ival[0] : 1000;
    int amp = tone_args.amp->count ? tone_args.amp->ival[0] : 30;
    amp_control_set_mute(false);
    esp_err_t err = i2s_dma_test_tone(hz, ms, amp);
    printf("tone %d Hz %d ms amp=%d%% -> %s\n", hz, ms, amp, esp_err_to_name(err));
    return 0;
}

/* --- volume <0..100> ------------------------------------------------------ */
static struct { struct arg_int *v; struct arg_end *end; } vol_args;

static int cmd_volume(int argc, char **argv)
{
    int nerr = arg_parse(argc, argv, (void **)&vol_args);
    if (nerr) {
        printf("volume = %d%%\n", volume_ctrl_get_percent());
        return 0;
    }
    int v = vol_args.v->ival[0];
    volume_ctrl_set_percent(v);
    nvs_cfg_set_volume(v);
    printf("volume = %d%%\n", v);
    return 0;
}

/* --- source <wifi|bt|none> ------------------------------------------------ */
static int cmd_source(int argc, char **argv)
{
    if (argc < 2) {
        printf("source = %s\n", source_manager_name(source_manager_get()));
        return 0;
    }
    audio_source_t s = SRC_NONE;
    if      (!strcmp(argv[1], "wifi")) s = SRC_WIFI_UDP;
    else if (!strcmp(argv[1], "bt"))   s = SRC_BT_A2DP;
    else if (!strcmp(argv[1], "none")) s = SRC_NONE;
    else { printf("usage: source <wifi|bt|none>\n"); return 1; }
    source_manager_select(s);
    printf("source -> %s\n", source_manager_name(s));
    return 0;
}

/* --- info / heap / reboot ------------------------------------------------- */
static int cmd_info(int argc, char **argv)
{
    (void)argc; (void)argv;
    char ip[16];
    wifi_sta_get_ip(ip);
    i2s_dma_stats_t st;
    i2s_dma_get_stats(&st);
    printf("SokolAudio Smart Amp\n");
    printf("  source  : %s\n", source_manager_name(source_manager_get()));
    printf("  volume  : %d%% (mute=%d)\n", volume_ctrl_get_percent(), amp_control_is_muted());
    printf("  wifi    : %s (ip=%s)\n", wifi_sta_is_connected() ? "up" : "down", ip);
    printf("  bt      : %s\n", bt_a2dp_sink_is_connected() ? "connected" : "idle");
    printf("  udp rx  : pushed=%u played=%u underruns=%u loss=%u free=%u\n",
           (unsigned)st.bytes_pushed, (unsigned)st.bytes_played,
           (unsigned)st.underruns, (unsigned)udp_receiver_get_packet_loss(),
           (unsigned)st.ringbuf_free);
    return 0;
}

static int cmd_heap(int argc, char **argv) { (void)argc; (void)argv; heap_debug_log_now(); return 0; }

static int cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("rebooting...\n");
    esp_restart();
    return 0;
}

/* --- wifi set <ssid> <pass> ----------------------------------------------- */
static int cmd_wifi(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "set")) {
        wifi_sta_set_credentials(argv[2], argv[3]);
        printf("wifi credentials saved; `reboot` to apply.\n");
        return 0;
    }
    printf("usage: wifi set <ssid> <password>\n");
    return 1;
}

esp_err_t serial_cli_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    cfg.prompt = "sokol> ";
    cfg.max_cmdline_length = 128;

    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&uart_cfg, &cfg, &repl),
                        TAG, "repl uart");

    tone_args.hz  = arg_int0(NULL, NULL, "<hz>", "tone frequency (default 1000)");
    tone_args.ms  = arg_int0(NULL, NULL, "<ms>", "duration ms (default 1000)");
    tone_args.amp = arg_int0(NULL, NULL, "<amp%>", "amplitude 0..100 (default 30)");
    tone_args.end = arg_end(2);
    const esp_console_cmd_t tone_cmd = {
        .command = "tone", .help = "Generate sine tone on I2S output",
        .hint = NULL, .func = &cmd_tone, .argtable = &tone_args,
    };
    esp_console_cmd_register(&tone_cmd);

    vol_args.v = arg_int0(NULL, NULL, "<0-100>", "target volume");
    vol_args.end = arg_end(2);
    const esp_console_cmd_t vol_cmd = {
        .command = "volume", .help = "Get/set volume percent",
        .hint = NULL, .func = &cmd_volume, .argtable = &vol_args,
    };
    esp_console_cmd_register(&vol_cmd);

    const esp_console_cmd_t src_cmd = {
        .command = "source", .help = "Select audio source (wifi|bt|none)",
        .hint = NULL, .func = &cmd_source, .argtable = NULL,
    };
    esp_console_cmd_register(&src_cmd);

    const esp_console_cmd_t info_cmd = { "info", "Show device status", NULL, &cmd_info, NULL };
    esp_console_cmd_register(&info_cmd);

    const esp_console_cmd_t heap_cmd = { "heap", "Log heap/PSRAM usage", NULL, &cmd_heap, NULL };
    esp_console_cmd_register(&heap_cmd);

    const esp_console_cmd_t wifi_cmd = { "wifi", "wifi set <ssid> <pass>", NULL, &cmd_wifi, NULL };
    esp_console_cmd_register(&wifi_cmd);

    const esp_console_cmd_t reboot_cmd = { "reboot", "Reboot the device", NULL, &cmd_reboot, NULL };
    esp_console_cmd_register(&reboot_cmd);

    esp_console_register_help_command();
    return esp_console_start_repl(repl);
}

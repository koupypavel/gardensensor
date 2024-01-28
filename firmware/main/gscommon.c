#include "gscommon.h"


int parse_ini_file(dictionary *ini, gs_configuration *config)
{
    config->mode = iniparser_getint(ini, "esp:mode", 0);

    config->wifi_ssid = iniparser_getstring(ini, "wifi:ssid", "CatNet");
    config->wifi_key = iniparser_getstring(ini, "wifi:key", "test");
    // iniparser_dump(ini, stdout);

    return 0;
}

int create_ini_file()
{
    FILE *ini;

    if ((ini = fopen("/data/config.ini", "w")) == NULL)
    {
        fprintf(stderr, "iniparser: cannot create config.ini\n");
        return -1;
    }

    fprintf(ini,
            "\n"
            "[esp]\n"
            "\n"
            "mode = 0 \n"
            "[wifi]\n"
            "\n"
            "mode = 0 \n"
            "ap_ssid = CatNet \n"
            "ap_key = test. \n"
            "\n");
    fclose(ini);
    return 0;
}
int save_ini_file(dictionary *setup)
{
    FILE *ini;

    if ((ini = fopen("/data/config.ini", "w")) == NULL)
    {
        ESP_LOGE("TAG", "iniparser: cannot create config.ini");
        return -1;
    }

    iniparser_dump_ini(setup, ini);
    fclose(ini);

    return 0;
}
void uart_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_RTS,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024 * 1, 1024 * 1, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 21, 20, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

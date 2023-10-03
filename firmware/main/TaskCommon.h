#pragma once

#include "string.h"
#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_vfs.h"
#include "esp_rom_gpio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "driver/touch_pad.h"
#include "driver/rtc_io.h"

#include "lwip/ip4_addr.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"

#include "Wifi.h"
#include "iniparser.h"
#include "MSCStorage.h"


/*#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"*/
enum Language{
    LANG_EN = 1,
    LANG_CS = 2
};

static const int RX_BUF_SIZE = 1024;

#define DAY2SECONDS 86400
#define DISCHARGE_LVL 2150

/************************************************************** TinyUSB ********/
/************* TinyUSB descriptors ****************/
#define EPNUM_MSC       1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)
#define BASE_PATH "/sdcard" // base path to mount the partition

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,

    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};
/************************************************************** TinyUSB ********/


typedef struct
{
    bool web_run;
    int web_timeout;
    int timestamp;
    enum Language lang;

    char *wifi_ssid;
    char *wifi_key;
    int wifi_mode;

    bool debug;;

} settings_st;

enum State
{
    CHARGING = 3,
    RUN_REMOTE_AUTH = 2,
    RUN = 1,
    DEEP_SLEEP = 0
};

extern SemaphoreHandle_t xMutex;
// extern SemaphoreHandle_t xMutex_status;
extern uint8_t flags;
extern int tdk_error;
extern bool discharged;
extern enum State esp_mode;
extern bool is_eject;

void initFATFS(bool mount2app);

void writeFlags(uint8_t flag_usr);
int save_ini_file(dictionary *setup);
int parse_ini_file(dictionary *setup, settings_st *config);

void start_deep_sleep(void);
esp_err_t setRTCTime(time_t epoch_ts);
int getRTCTime(void);
int create_ini_file();

#define WEB_FLAGS(byte) (byte & 0xF0)

#define HIGH 1
#define LOW 0
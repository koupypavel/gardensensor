#pragma once

#ifndef __GS_COMMON_H__
#define __GS_COMMON_H__

#include "version.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "config/dictionary.h"
#include "config/iniparser.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_spiffs.h"

#include "driver/uart.h"


typedef enum
{
    STATE_MQTT_GENERIC = 0,
    STATE_HA_ENTITY = 1,
    STATE_RECHARGE = 2,
} gs_state;

typedef struct 
{
    char * wifi_ssid;
    char * wifi_key;

    int mode;


} gs_configuration;

/************ Humidity sensor #1 ****************/
#define GPIO_H1_D              (GPIO_NUM_6) 
#define GPIO_H1_A              (GPIO_NUM_7)
#define GPIO_H1_A_CHANNEL        (ADC_CHANNEL_6)
/************ Humidity sensor #2 ****************/
#define GPIO_H2_D      (GPIO_NUM_10)
#define GPIO_H2_A       (GPIO_NUM_11)
#define GPIO_H2_A_CHANNEL        (ADC_CHANNEL_0)
/************ Humidity sensor #3 ****************/ //J10
#define GPIO_H3_D      (GPIO_NUM_12)
#define GPIO_H3_A       (GPIO_NUM_13)
#define GPIO_H3_A_CHANNEL        (ADC_CHANNEL_2)
/************ Humidity sensor #4 ****************/ //the one right next to LDO J11
#define GPIO_H4_D       (GPIO_NUM_21)
#define GPIO_H4_A      (GPIO_NUM_14)
#define GPIO_H4_A_CHANNEL        (ADC_CHANNEL_3)

#ifdef __cplusplus
extern "C" {
#endif

int parse_ini_file(dictionary *ini, gs_configuration *config);
int create_ini_file();
void uart_init();
int save_ini_file(dictionary *setup);

#ifdef __cplusplus
}
#endif

#endif // __GS_COMMON_H__

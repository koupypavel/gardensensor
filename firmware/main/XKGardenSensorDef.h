#pragma once

#include "version.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

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

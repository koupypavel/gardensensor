#pragma once

#include "gscommon.h"
// ADC Channels
// ADC Attenuation

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_EXAMPLE_ATTEN ADC_ATTEN_DB_11
typedef enum 
{
    HUMIDITY_SENSOR_1 = 0x01,
    HUMIDITY_SENSOR_2 = 0x02,
    HUMIDITY_SENSOR_3 = 0x03,
    HUMIDITY_SENSOR_4 = 0x04,
}humiditySensorIndex;

int measure_batt_lvl();
int measure_humidity_lvl(humiditySensorIndex index);
void init_ADC();
void deinit_ADC();
bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle);

#ifdef __cplusplus
}
#endif

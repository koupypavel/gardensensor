#include "StatusTask.h"

static bool cali_enable = false;

uint8_t status = 0;
static const char *TAG = "StatusTask";

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t handle = NULL;


/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, out_handle);
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "calibrated = true;");
            calibrated = true;
        }
        else
        {
            ESP_LOGI(TAG, "calibrated = false;");
        }
    }

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

void init_ADC()
{
    //-------------ADC1 Init---------------//

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11,
    };

    //ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_EXAMPLE_CHAN0, &config));
    // calibration routine to get mV from raw reading
    cali_enable = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &handle);
}

void deinit_ADC()
{
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (cali_enable)
    {
        ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
    }
}

int measure_batt_lvl()
{
    int voltage = 0;
    int adc_raw = 0;
   // ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_EXAMPLE_CHAN0, &adc_raw));
    if (cali_enable)
    {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(handle, adc_raw, &voltage));
       // ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC1_EXAMPLE_CHAN0, voltage);
    }
    return voltage;
}

int measure_humidity_lvl(humiditySensorIndex index)
{
    int voltage = 0;
    int adc_raw = 0;
   // ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_EXAMPLE_CHAN0, &adc_raw));
    if (cali_enable)
    {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(handle, adc_raw, &voltage));
       // ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, ADC1_EXAMPLE_CHAN0, voltage);
    }
    return voltage;
}

void StatusTask(void *arg)
{
    bool blink = true;
    init_ADC();
   // uint32_t batt_level = measure_batt_lvl();
    bool dwn_done = false;

    while (true)
    {
        /*if (batt_level < DISCHARGE_LVL)
        {
            discharged = true;
        }*/

        if (esp_mode == DEEP_SLEEP)
        {
            // when charging show up charging status every 5 minutes
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            //uint32_t batt_level = measure_batt_lvl();
            continue;
        }
        else if (esp_mode == RUN)
        {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
        else if (esp_mode == CHARGING)
        {
            // when charging show up charging status every 5 minutes
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            //uint32_t batt_level = measure_batt_lvl();
            continue;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    deinit_ADC();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}
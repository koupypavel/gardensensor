#include "stdio.h"
#include "version.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include "gscommon.h"
#include "mqtt_task.h"
#include "sensor.h"
#include "Wifi.h"

#include <HaBridge.h>
#include <MQTTRemote.h>
#include <driver/gpio.h>
#include <entities/HaEntityHumidity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nlohmann/json.hpp>

static const char *TAG = "Garden sensor";
const char *config_path = "/data/config.ini";
SemaphoreHandle_t xMutex = NULL;
bool discharged = false;

gs_state esp_mode = STATE_MQTT_GENERIC;
uint8_t flags = 0;
int tdk_error = 0;

FILE *_log_remote_fp;
bool _log2stdout = false;
bool _log2file = true;

const char mqtt_client_id[] = "example";
const char mqtt_host[] = "192.168.1.123";
const char mqtt_username[] = "";
const char mqtt_password[] = "";

nlohmann::json _json_this_device_doc;
void setupJsonForThisDevice()
{
    _json_this_device_doc["identifiers"] = "gsensor_" + std::string(mqtt_client_id);
    _json_this_device_doc["name"] = "GardenSensor";
    _json_this_device_doc["sw_version"] = "1.0.0";
    _json_this_device_doc["model"] = "v1.0.0";
    _json_this_device_doc["manufacturer"] = "PaKo";
}

extern "C" void app_main(void)
{

 bool default_reset = false;
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/data",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_vfs_spiffs_register(&conf);
    // load settings
    dictionary *setup_config;
    gs_configuration setup;

    setup_config = iniparser_load("/data/config.ini");

    if (setup_config == NULL)
    {
        ESP_LOGE("FileServer", "setup config fail");
        create_ini_file();
        setup_config = iniparser_load("/data/config.ini");
    }

    parse_ini_file(setup_config, &setup);

    switch (esp_mode)
    {
    case STATE_MQTT_GENERIC:
    {
        ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
        esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
        esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
        esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
        esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
        esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
        esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        wifi_init_sta("CatNet", "test.");

        mqtt5_app_start();

        while (true)
        {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
    }
    break;
    case STATE_HA_ENTITY:
    {
        MQTTRemote _mqtt_remote(mqtt_client_id, mqtt_host, 1883, mqtt_username, mqtt_password, 2048, 10);
        HaBridge ha_bridge(_mqtt_remote, "GardenSensor", _json_this_device_doc);
        HaEntityHumidity _ha_entity_sensor_a(ha_bridge, "sensor a", "gsensor_sen_a");
        HaEntityHumidity _ha_entity_sensor_b(ha_bridge, "sensor b", "gsensor_sen_b");
        HaEntityHumidity _ha_entity_sensor_c(ha_bridge, "sensor c", "gsensor_sen_c");
        HaEntityHumidity _ha_entity_sensor_d(ha_bridge, "sensor d", "gsensor_sen_d");
        while (1)
        {
            _ha_entity_sensor_a.publishHumidity(50);
            _ha_entity_sensor_b.publishHumidity(50);
            _ha_entity_sensor_c.publishHumidity(50);
            _ha_entity_sensor_d.publishHumidity(50);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
    break;
    default:
    {
        while (true)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    break;
    }
    ESP_LOGI(TAG, "Entering deep sleep");
    // start_deep_sleep();
}

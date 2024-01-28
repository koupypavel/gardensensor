idf_component_register(SRCS "mqtt_task.c" "sensor.c" "main.cpp"  "wifi.c" "gscommon.c"  "config/dictionary.c" "config/iniparser.c"
                    INCLUDE_DIRS "." "../build"
                    REQUIRES esp_system esp_rom nvs_flash driver esp_wifi spiffs esp_adc vfs spi_flash esp_partition efuse soc app_update mqtt cxx)



idf_component_register(SRCS "MSCStorage.c" "StatusTask.c" "DNSServerTask.c" "FileServer.c" "main.c"  "Wifi.c" "FileHandler.c" "TaskCommon.c" "FileAccessTask.c" "FileServer.c" "DNSServerTask.c" "dictionary.c" "iniparser.c"
                    INCLUDE_DIRS "." "../build"
                    EMBED_FILES "frontend/favicon.ico" "frontend/canlab.png" "frontend/admin.css" "frontend/table.js" "frontend/setting.js" "frontend/admin.html" "frontend/setting.html" "frontend/update.html" "frontend/download.html"
                    REQUIRES esp_system esp_rom nvs_flash lwip driver esp_wifi esp_adc vfs spi_flash esp_partition efuse esp_http_server soc fatfs app_update mqtt )



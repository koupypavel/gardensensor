#pragma once
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#define EXAMPLE_ESP_WIFI_SSID "TachoDownloadKey"
#define EXAMPLE_ESP_WIFI_PASS "canlab2541"
#define EXAMPLE_ESP_WIFI_CHANNEL 0
#define EXAMPLE_MAX_STA_CONN 2
#ifdef __cplusplus
extern "C" {
#endif

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

void wifi_init_ap(char* ssid, char* password);
bool wifi_init_sta(char* ssid, char* passwor);
#ifdef __cplusplus
}
#endif
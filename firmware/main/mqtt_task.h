#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#ifndef __GS_MQTT_TASK_H__
#define __GS_MQTT_TASK_H__


#ifdef __cplusplus
extern "C" {
#endif
void mqtt5_app_start(void);


#ifdef __cplusplus
}
#endif


#endif //__GS_MQTT_TASK_H__
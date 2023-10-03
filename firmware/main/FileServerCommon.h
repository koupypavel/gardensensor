
#pragma once

#include "sdkconfig.h"
#include "esp_err.h"

//esp_err_t download_get_handler(httpd_req_t *req);


#ifdef __cplusplus
extern "C" {
#endif

esp_err_t start_server();
esp_err_t start_file_server(const char *base_path);
 
#ifdef __cplusplus
}
#endif

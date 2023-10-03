#include "FileAccessTask.h"

void FileAccessTask(void *arg)
{
     start_server();
     start_file_server("/data");

     while (true)
          vTaskDelay(5000 / portTICK_PERIOD_MS);

     ESP_LOGE("GPIO", "tacho key disconnected form VU or charging cable disconected");
     // } while (VU_FLAGS(flags) != FLAGS_TASK_VU_FINISHED);
     // write end of vu session into general flags, so timeout on web can start
     vTaskDelay(1000 / portTICK_PERIOD_MS);
     vTaskDelete(NULL);
}
#ifndef MAIN_HTTP_H_
#define MAIN_HTTP_H_
//#define LOG_LOCAL_LEVEL ESP_LOG_ERROR
//#define LOG_LOCAL_LEVEL ESP_LOG_INFO
//-------------------------------------------------------------
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "temperature.h"

// семафор раздельного обращения к данным температуры:
extern SemaphoreHandle_t temperature_data_sem;

//-------------------------------------------------------------
void http_task(void *pvParameters);
//-------------------------------------------------------------
#endif /* MAIN_HTTP_H_ */

#ifndef MAIN_HTTP_H_
#define MAIN_HTTP_H_
//-------------------------------------------------------------
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "temperature.h"

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

// семафор раздельного обращения к данным температуры:
extern SemaphoreHandle_t temperature_data_sem;

//-------------------------------------------------------------
void http_task(void *pvParameters);
// формируем страничку index.json со статистикой для машины:
char *create_json(TEMPERATURE_data* td);
// формируем страничку index.html со статистикой для человека:
char* create_index(TEMPERATURE_data *td);
//-------------------------------------------------------------
#endif /* MAIN_HTTP_H_ */

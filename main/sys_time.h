#ifndef MAIN_SYS_TIME_H_
#define MAIN_SYS_TIME_H_
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#define NTP_WAIT_INFINITY 0

// старт получения времени по сети:
void esp_initialize_sntp(void);
// ожидание успешного получения времени по сети:
bool esp_wait_sntp_sync(int wait_seconds);
// возвращает строку с описанием времени работы системы с этапа загрузки (дни, часты, минуты, секунды), 
// возвращаемую строку нужно очистить через free()
char* systime_uptime(void);
#endif /* MAIN_SYS_TIME_H_ */

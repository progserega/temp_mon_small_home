#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_
//---------------------------------------------------------------------
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "owb.h"
#include "ds18b20.h"

#define ERROR_TEMPERATURE -1000
typedef struct {
  char device_addr[OWB_ROM_CODE_STRING_LENGTH]; // адрес устройства на шине 1-wire
  char *device_name; // символичное имя устройства (может назначаться извне)
  float temp; // последняя прочитанная температура
  int errors; // количество ошибок чтения
}TEMPERATURE_device ;
// поиск всех датчиков температуры (DS1820), подключённых к шине и их инициализация:
int temperature_init_devices(void);
int temperature_deactivate_devices(void);
int temperature_update_device_data(void);
struct TEMPERATURE_device * temperature_get_devices(void);
//---------------------------------------------------------------------

#endif /* TEMPERATURE_H_ */


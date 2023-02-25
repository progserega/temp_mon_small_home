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
  char device_name[17]; // символичное имя устройства (может назначаться извне)
  float temp; // последняя прочитанная температура
  int errors; // количество ошибок чтения
}TEMPERATURE_device ;

typedef struct {
  // шина 1-wire:
  OneWireBus * owb;
  // rmt driver structure:
  owb_rmt_driver_info *rmt_driver_info;
  // 1-wire устройства:
  OneWireBus_ROMCode *device_rom_codes;
  // DS1820 датчики:
  DS18B20_Info *devices;
  // количество найденных датчиков:
  int num_devices;
  // массив с данными устройств, выдаваемый наверх
  TEMPERATURE_device *temp_devices;
}TEMPERATURE_data;

// поиск всех датчиков температуры (DS1820), подключённых к шине и их инициализация:
TEMPERATURE_data* temperature_init_devices(void);
int temperature_deactivate_devices(TEMPERATURE_data *td);
int temperature_update_device_data(TEMPERATURE_data *td);
//---------------------------------------------------------------------

#endif /* TEMPERATURE_H_ */


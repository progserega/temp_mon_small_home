#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_
//---------------------------------------------------------------------
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "owb.h"
#include "ds18b20.h"
#include "esp_log.h"

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define ERROR_TEMPERATURE -1000000
typedef struct {
  // температура хранится в приведённом виде: (int)(float*1000),
  // т.к. температура может быть в достаточно узком диапазоне значений от -100 до +100 с 
  // погрешностью до 0.1 C, то отдавать под неё 7 байт float - нет смысла - особенно для
  // статистических данных
  int min; 
  int max; 
} TEMPERATURE_stat_item;

typedef struct {
  char device_addr[OWB_ROM_CODE_STRING_LENGTH]; // адрес устройства на шине 1-wire
  char device_name[17]; // символичное имя устройства (может назначаться извне)
  float temp; // последняя прочитанная температура
  int errors; // количество ошибок чтения

  ///==== Статистика температуры: ====
  // статистика за сутки:
  TEMPERATURE_stat_item stat_day[24]; // статистика хранится в приведённом виде (int) за каждый час
  int stat_day_max_temp;
  int stat_day_min_temp;
  // статистика за месяц:
  TEMPERATURE_stat_item stat_month[31]; // статистика хранится в приведённом виде (int) за каждые сутки
  int stat_month_max_temp;
  int stat_month_min_temp;
  // статистика за год:
  TEMPERATURE_stat_item stat_year[12]; // статистика хранится в приведённом виде (int) за каждый месяц
  int stat_year_max_temp;
  int stat_year_min_temp;
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
  // флаг актуальности внтуренних часов устройства (время обновляется по сети с ntp-сервера):
  bool time_updated;
}TEMPERATURE_data;

// поиск всех датчиков температуры (DS1820), подключённых к шине и их инициализация:
TEMPERATURE_data* temperature_init_devices(void);
int temperature_deactivate_devices(TEMPERATURE_data *td);
// обновление данных по датчикам:
int temperature_update_device_data(TEMPERATURE_data *td);
// прописывание имён датчикам:
void add_alias_to_temp_devices(TEMPERATURE_data *td);
// обновление статистики температуры - суточной, месяцной, за год:
int temperature_update_device_stat(TEMPERATURE_data *td);
// заполнение структур температур ошибочными значениями:
void reset_temperature(TEMPERATURE_data *td);
// сохранение на внутренний flash-носитель статистических данных:
int temperature_stat_save_to_flash(TEMPERATURE_data *td);
// восстановление с внутреннего flash-носителя статистических данных:
int temperature_stat_load_from_flash(TEMPERATURE_data *td);
//---------------------------------------------------------------------

#endif /* TEMPERATURE_H_ */


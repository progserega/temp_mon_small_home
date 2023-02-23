#ifndef MAIN_DS18B20_H_
#define MAIN_DS18B20_H_
//---------------------------------------------------------------------
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "owb.h"
//---------------------------------------------------------------------
typedef enum
{
  DS18B20_RESOLUTION_INVALID = -1,  ///< Invalid resolution
  DS18B20_RESOLUTION_9_BIT   = 9,   ///< 9-bit resolution, LSB bits 2,1,0 undefined
  DS18B20_RESOLUTION_10_BIT  = 10,  ///< 10-bit resolution, LSB bits 1,0 undefined
  DS18B20_RESOLUTION_11_BIT  = 11,  ///< 11-bit resolution, LSB bit 0 undefined
  DS18B20_RESOLUTION_12_BIT  = 12,  ///< 12-bit resolution (default)
} DS18B20_RESOLUTION;
//---------------------------------------------------------------------
typedef struct
{
  bool init;                     ///< True if struct has been initialised, otherwise false
  bool solo;                     ///< True if device is intended to be the only one connected to the bus, otherwise false
  bool use_crc;                  ///< True if CRC checks are to be used when retrieving information from a device on the bus
  const OneWireBus * bus;        ///< Pointer to 1-Wire bus information relevant to this device
  OneWireBus_ROMCode rom_code;   ///< The ROM code used to address this device on the bus
  DS18B20_RESOLUTION resolution; ///< Temperature measurement resolution per reading
} DS18B20_Info;
//---------------------------------------------------------------------
typedef enum
{
    DS18B20_ERROR_UNKNOWN = -1,  ///< An unknown error occurred, or the value was not set
    DS18B20_OK = 0,        ///< Success
    DS18B20_ERROR_DEVICE,  ///< A device error occurred
    DS18B20_ERROR_CRC,     ///< A CRC error occurred
    DS18B20_ERROR_OWB,     ///< A One Wire Bus error occurred
    DS18B20_ERROR_NULL,    ///< A parameter or value is NULL

} DS18B20_ERROR;
//---------------------------------------------------------------------
DS18B20_Info * ds18b20_malloc(void);
void ds18b20_use_crc(DS18B20_Info * ds18b20_info, bool use_crc);
bool ds18b20_set_resolution(DS18B20_Info * ds18b20_info, DS18B20_RESOLUTION resolution);
void ds18b20_convert_all(const OneWireBus * bus);
float ds18b20_wait_for_conversion(const DS18B20_Info * ds18b20_info);
DS18B20_ERROR ds18b20_read_temp(const DS18B20_Info * ds18b20_info, float * value);
DS18B20_ERROR ds18b20_check_for_parasite_power(const OneWireBus * bus, bool * present);
void ds18b20_free(DS18B20_Info ** ds18b20_info);
void ds18b20_init(DS18B20_Info * ds18b20_info, const OneWireBus * bus, OneWireBus_ROMCode rom_code);
void ds18b20_init_solo(DS18B20_Info * ds18b20_info, const OneWireBus * bus);
//---------------------------------------------------------------------
#endif /* MAIN_DS18B20_H_ */

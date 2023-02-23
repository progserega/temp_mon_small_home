#ifndef MAIN_OWB_H_
#define MAIN_OWB_H_
//---------------------------------------------------------------------
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "driver/rmt.h"
#include "driver/gpio.h"
//---------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
//---------------------------------------------------------------------
#define OWB_ROM_SEARCH        0xF0  ///< Perform Search ROM cycle to identify devices on the bus
#define OWB_ROM_READ          0x33  ///< Read device ROM (single device on bus only)
#define OWB_ROM_MATCH         0x55  ///< Address a specific device on the bus by ROM
#define OWB_ROM_SKIP          0xCC  ///< Address all devices on the bus simultaneously
//---------------------------------------------------------------------
#define OWB_ROM_CODE_STRING_LENGTH (17)  ///< Typical length of OneWire bus ROM ID as ASCII hex string, including null terminator
//---------------------------------------------------------------------
typedef union
{
  // Provides access via field names
  struct fields
  {
    uint8_t family[1];         ///< family identifier (1 byte, LSB - read/write first)
    uint8_t serial_number[6];  ///< serial number (6 bytes)
    uint8_t crc[1];            ///< CRC check byte (1 byte, MSB - read/write last)
  } fields;                      ///< Provides access via field names
  uint8_t bytes[8];              ///< Provides raw byte access
} OneWireBus_ROMCode;
//---------------------------------------------------------------------
typedef struct
{
    OneWireBus_ROMCode rom_code;   ///< Device ROM code
    int last_discrepancy;          ///< Bit index that identifies from which bit the next search discrepancy check should start
    int last_family_discrepancy;   ///< Bit index that identifies the last discrepancy within the first 8-bit family code of the ROM code
    int last_device_flag;          ///< Flag to indicate previous search was the last device detected
} OneWireBus_SearchState;
//---------------------------------------------------------------------
typedef struct
{
  const struct _OneWireBus_Timing * timing;   ///< Pointer to timing information
  bool use_crc;                               ///< True if CRC checks are to be used when retrieving information from a device on the bus
  bool use_parasitic_power;                   ///< True if parasitic-powered devices are expected on the bus
  gpio_num_t strong_pullup_gpio;              ///< Set if an external strong pull-up circuit is required
  const struct owb_driver * driver;           ///< Pointer to hardware driver instance
} OneWireBus;
//---------------------------------------------------------------------
typedef struct
{
  int tx_channel;     ///< RMT channel to use for TX
  int rx_channel;     ///< RMT channel to use for RX
  RingbufHandle_t rb; ///< Ring buffer handle
  int gpio;           ///< OneWireBus GPIO
  OneWireBus bus;     ///< OneWireBus instance
} owb_rmt_driver_info;
//---------------------------------------------------------------------
typedef enum
{
  OWB_STATUS_NOT_SET = -1,           ///< A status value has not been set
  OWB_STATUS_OK = 0,                 ///< Operation succeeded
  OWB_STATUS_NOT_INITIALIZED,        ///< Function was passed an uninitialised variable
  OWB_STATUS_PARAMETER_NULL,         ///< Function was passed a null pointer
  OWB_STATUS_DEVICE_NOT_RESPONDING,  ///< No response received from the addressed device or devices
  OWB_STATUS_CRC_FAILED,             ///< CRC failed on data received from a device or devices
  OWB_STATUS_TOO_MANY_BITS,          ///< Attempt to write an incorrect number of bits to the One Wire Bus
  OWB_STATUS_HW_ERROR                ///< A hardware error occurred
} owb_status;
//--------------------------------------------------------------------
struct _OneWireBus_Timing
{
  uint32_t A, B, C, D, E, F, G, H, I, J;
};
//---------------------------------------------------------------------
struct owb_driver
{
  /** Driver identification **/
  const char* name;

  /** Pointer to driver uninitialization function **/
  owb_status (*uninitialize)(const OneWireBus * bus);

  /** Pointer to driver reset functio **/
  owb_status (*reset)(const OneWireBus * bus, bool *is_present);

  /** NOTE: The data is shifted out of the low bits, eg. it is written in the order of lsb to msb */
  owb_status (*write_bits)(const OneWireBus *bus, uint8_t out, int number_of_bits_to_write);

  /** NOTE: Data is read into the high bits, eg. each bit read is shifted down before the next bit is read */
  owb_status (*read_bits)(const OneWireBus *bus, uint8_t *in, int number_of_bits_to_read);
};
//---------------------------------------------------------------------
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
//---------------------------------------------------------------------
owb_status owb_use_crc(OneWireBus * bus, bool use_crc);
uint8_t owb_crc8_bytes(uint8_t crc, const uint8_t * data, size_t len);
owb_status owb_search_first(const OneWireBus * bus, OneWireBus_SearchState * state, bool * found_device);
owb_status owb_search_next(const OneWireBus * bus, OneWireBus_SearchState * state, bool * found_device);
char * owb_string_from_rom_code(OneWireBus_ROMCode rom_code, char * buffer, size_t len);
owb_status owb_read_bit(const OneWireBus * bus, uint8_t * out);
owb_status owb_read_bytes(const OneWireBus * bus, uint8_t * buffer, unsigned int len);
owb_status owb_write_byte(const OneWireBus * bus, uint8_t data);
owb_status owb_write_bytes(const OneWireBus * bus, const uint8_t * buffer, size_t len);
owb_status owb_write_rom_code(const OneWireBus * bus, OneWireBus_ROMCode rom_code);
owb_status owb_use_parasitic_power(OneWireBus * bus, bool use_parasitic_power);
owb_status owb_read_rom(const OneWireBus * bus, OneWireBus_ROMCode *rom_code);
owb_status owb_verify_rom(const OneWireBus * bus, OneWireBus_ROMCode rom_code, bool * is_present);
owb_status owb_reset(const OneWireBus * bus, bool * a_device_present);
owb_status owb_set_strong_pullup(const OneWireBus * bus, bool enable);
OneWireBus * owb_rmt_initialize(owb_rmt_driver_info * info, gpio_num_t gpio_num,
                                rmt_channel_t tx_channel, rmt_channel_t rx_channel);
owb_status owb_uninitialize(OneWireBus * bus);
//---------------------------------------------------------------------
#endif /* MAIN_OWB_H_ */

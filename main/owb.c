#include "owb.h"
//---------------------------------------------------------------------
static const char *TAG = "owb";
//---------------------------------------------------------------------
//#define OW_DEBUG
//---------------------------------------------------------------------
#define OW_DURATION_RESET 480
#define MAX_BITS_PER_SLOT (8)
#define OW_DURATION_SLOT 75
#define OW_DURATION_1_LOW    2
#define OW_DURATION_1_HIGH (OW_DURATION_SLOT - OW_DURATION_1_LOW)
#define OW_DURATION_0_LOW   65
#define OW_DURATION_0_HIGH (OW_DURATION_SLOT - OW_DURATION_0_LOW)
#define OW_DURATION_RX_IDLE (OW_DURATION_SLOT + 2)
#define OW_DURATION_SAMPLE  (15-2)
//--------------------------------------------------------------------
#define info_of_driver(owb) container_of(owb, owb_rmt_driver_info, bus)
//---------------------------------------------------------------------
static uint8_t _calc_crc(uint8_t crc, uint8_t data)
{
    static const uint8_t table[256] = {
            0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
            157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
            35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
            190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
            70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
            219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
            101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
            248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
            140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
            17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
            175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
            50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
            202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
            87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
            233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
            116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
    };

    return table[crc ^ data];
}
//---------------------------------------------------------------------
static uint8_t _calc_crc_block(uint8_t crc, const uint8_t * buffer, size_t len)
{
    do
    {
        crc = _calc_crc(crc, *buffer++);
        ESP_LOGD(TAG, "buffer 0x%02x, crc 0x%02x, len %d", (uint8_t)*(buffer - 1), (int)crc, (int)len);
    }
    while (--len > 0);
    return crc;
}
//---------------------------------------------------------------------
static bool _is_init(const OneWireBus * bus)
{
  bool ok = false;
  if (bus != NULL)
  {
    if (bus->driver)
    {
      // OK
      ok = true;
    }
    else
    {
      ESP_LOGE(TAG, "bus is not initialised");
    }
  }
  else
  {
    ESP_LOGE(TAG, "bus is NULL");
  }
  return ok;
}
//---------------------------------------------------------------------
owb_status owb_use_crc(OneWireBus * bus, bool use_crc)
{
    owb_status status = OWB_STATUS_NOT_SET;

    if (!bus)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        bus->use_crc = use_crc;
        ESP_LOGD(TAG, "use_crc %d", bus->use_crc);

        status = OWB_STATUS_OK;
    }

    return status;
}
//---------------------------------------------------------------------
static void onewire_flush_rmt_rx_buf(const OneWireBus * bus)
{
    void * p = NULL;
    size_t s = 0;

    owb_rmt_driver_info * i = info_of_driver(bus);

    while ((p = xRingbufferReceive(i->rb, &s, 0)))
    {
        ESP_LOGD(TAG, "flushing entry");
        vRingbufferReturnItem(i->rb, p);
    }
}
//---------------------------------------------------------------------
uint8_t owb_crc8_byte(uint8_t crc, uint8_t data)
{
    return _calc_crc(crc, data);
}
//---------------------------------------------------------------------
uint8_t owb_crc8_bytes(uint8_t crc, const uint8_t * data, size_t len)
{
    return _calc_crc_block(crc, data, len);
}
//---------------------------------------------------------------------
static owb_status _search(const OneWireBus * bus, OneWireBus_SearchState * state, bool * is_found)
{
  int id_bit_number = 1;
  int last_zero = 0;
  int rom_byte_number = 0;
  uint8_t id_bit = 0;
  uint8_t cmp_id_bit = 0;
  uint8_t rom_byte_mask = 1;
  uint8_t search_direction = 0;
  bool search_result = false;
  uint8_t crc8 = 0;
  owb_status status = OWB_STATUS_NOT_SET;

  // if the last call was not the last one
  if (!state->last_device_flag)
  {
    // 1-Wire reset
    bool is_present;
    bus->driver->reset(bus, &is_present);

    if (!is_present)
    {
        // reset the search
        state->last_discrepancy = 0;
        state->last_device_flag = false;
        state->last_family_discrepancy = 0;
        *is_found = false;
        return OWB_STATUS_OK;
    }

    // issue the search command
    bus->driver->write_bits(bus, OWB_ROM_SEARCH, 8);

    // loop to do the search
    do
    {
      id_bit = cmp_id_bit = 0;
      // read a bit and its complement
      bus->driver->read_bits(bus, &id_bit, 1);
      bus->driver->read_bits(bus, &cmp_id_bit, 1);

      // check for no devices on 1-wire (signal level is high in both bit reads)
      if (id_bit && cmp_id_bit)
      {
          break;
      }
      else
      {
        // all devices coupled have 0 or 1
        if (id_bit != cmp_id_bit)
        {
            search_direction = (id_bit) ? 1 : 0;  // bit write value for search
        }
        else
        {
          // if this discrepancy if before the Last Discrepancy
          // on a previous next then pick the same as last time
          if (id_bit_number < state->last_discrepancy)
          {
              search_direction = ((state->rom_code.bytes[rom_byte_number] & rom_byte_mask) > 0);
          }
          else
          {
              // if equal to last pick 1, if not then pick 0
              search_direction = (id_bit_number == state->last_discrepancy);
          }

          // if 0 was picked then record its position in LastZero
          if (search_direction == 0)
          {
              last_zero = id_bit_number;

              // check for Last discrepancy in family
              if (last_zero < 9)
              {
                  state->last_family_discrepancy = last_zero;
              }
          }
        }

        // set or clear the bit in the ROM byte rom_byte_number
        // with mask rom_byte_mask
        if (search_direction == 1)
        {
            state->rom_code.bytes[rom_byte_number] |= rom_byte_mask;
        }
        else
        {
            state->rom_code.bytes[rom_byte_number] &= ~rom_byte_mask;
        }

        // serial number search direction write bit
        bus->driver->write_bits(bus, search_direction, 1);

        // increment the byte counter id_bit_number
        // and shift the mask rom_byte_mask
        id_bit_number++;
        rom_byte_mask <<= 1;

        // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
        if (rom_byte_mask == 0)
        {
            crc8 = owb_crc8_byte(crc8, state->rom_code.bytes[rom_byte_number]);  // accumulate the CRC
            rom_byte_number++;
            rom_byte_mask = 1;
        }
      }
    } while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

    // if the search was successful then
    if (!((id_bit_number < 65) || (crc8 != 0)))
    {
      // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
      state->last_discrepancy = last_zero;

      // check for last device
      if (state->last_discrepancy == 0)
      {
          state->last_device_flag = true;
      }

      search_result = true;
    }
  }

  // if no device found then reset counters so next 'search' will be like a first
  if (!search_result || !state->rom_code.bytes[0])
  {
      state->last_discrepancy = 0;
      state->last_device_flag = false;
      state->last_family_discrepancy = 0;
      search_result = false;
  }

  status = OWB_STATUS_OK;

  *is_found = search_result;

  return status;
}
//---------------------------------------------------------------------
owb_status owb_search_first(const OneWireBus * bus, OneWireBus_SearchState * state, bool * found_device)
{
    bool result;
    owb_status status = OWB_STATUS_NOT_SET;

    if (!bus || !state || !found_device)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        memset(&state->rom_code, 0, sizeof(state->rom_code));
        state->last_discrepancy = 0;
        state->last_family_discrepancy = 0;
        state->last_device_flag = false;
        _search(bus, state, &result);
        status = OWB_STATUS_OK;

        *found_device = result;
    }

    return status;
}
//---------------------------------------------------------------------
owb_status owb_search_next(const OneWireBus * bus, OneWireBus_SearchState * state, bool * found_device)
{
    owb_status status = OWB_STATUS_NOT_SET;
    bool result = false;

    if (!bus || !state || !found_device)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        _search(bus, state, &result);
        status = OWB_STATUS_OK;

        *found_device = result;
    }

    return status;
}
//---------------------------------------------------------------------
char * owb_string_from_rom_code(OneWireBus_ROMCode rom_code, char * buffer, size_t len)
{
    for (int i = sizeof(rom_code.bytes) - 1; i >= 0; i--)
    {
        sprintf(buffer, "%02x", rom_code.bytes[i]);
        buffer += 2;
    }
    return buffer;
}
//---------------------------------------------------------------------
static owb_status _uninitialize(const OneWireBus *bus)
{
  owb_rmt_driver_info * info = info_of_driver(bus);

  rmt_driver_uninstall(info->tx_channel);
  rmt_driver_uninstall(info->rx_channel);

  return OWB_STATUS_OK;
}
//---------------------------------------------------------------------
static owb_status _reset(const OneWireBus * bus, bool * is_present)
{
  rmt_item32_t tx_items[1] = {0};
  bool _is_present = false;
  int res = OWB_STATUS_OK;
  owb_rmt_driver_info * i = info_of_driver(bus);

  tx_items[0].duration0 = OW_DURATION_RESET;
  tx_items[0].level0 = 0;
  tx_items[0].duration1 = 0;
  tx_items[0].level1 = 1;

  uint16_t old_rx_thresh = 0;
  rmt_get_rx_idle_thresh(i->rx_channel, &old_rx_thresh);
  rmt_set_rx_idle_thresh(i->rx_channel, OW_DURATION_RESET + 60);

  onewire_flush_rmt_rx_buf(bus);

  rmt_rx_start(i->rx_channel, true);

  if (rmt_write_items(i->tx_channel, tx_items, 1, true) == ESP_OK)
  {
    size_t rx_size = 0;
    rmt_item32_t * rx_items = (rmt_item32_t *)xRingbufferReceive(i->rb, &rx_size, 100 / portTICK_PERIOD_MS);

    if (rx_items)
    {
      if (rx_size >= (1 * sizeof(rmt_item32_t)))
      {
#ifdef OW_DEBUG
        ESP_LOGI(TAG, "rx_size: %d", rx_size);

        for (int i = 0; i < (rx_size / sizeof(rmt_item32_t)); i++)
        {
          ESP_LOGI(TAG, "i: %d, level0: %d, duration %d", i, rx_items[i].level0, rx_items[i].duration0);
          ESP_LOGI(TAG, "i: %d, level1: %d, duration %d", i, rx_items[i].level1, rx_items[i].duration1);
        }
#endif
        // parse signal and search for presence pulse
        if ((rx_items[0].level0 == 0) && (rx_items[0].duration0 >= OW_DURATION_RESET - 2))
        {
          if ((rx_items[0].level1 == 1) && (rx_items[0].duration1 > 0))
          {
            if (rx_items[1].level0 == 0)
            {
                _is_present = true;
            }
          }
        }
      }
      vRingbufferReturnItem(i->rb, (void *)rx_items);
    }
    else
    {
        // time out occurred, this indicates an unconnected / misconfigured bus
        ESP_LOGE(TAG, "rx_items == 0");
        res = OWB_STATUS_HW_ERROR;
    }
  }
  else
  {
      // error in tx channel
      ESP_LOGE(TAG, "Error tx");
      res = OWB_STATUS_HW_ERROR;
  }

  rmt_rx_stop(i->rx_channel);
  rmt_set_rx_idle_thresh(i->rx_channel, old_rx_thresh);

  *is_present = _is_present;

  ESP_LOGD(TAG, "_is_present %d", _is_present);

  return res;
}
//---------------------------------------------------------------------
static rmt_item32_t _encode_write_slot(uint8_t val)
{
    rmt_item32_t item = {0};

    item.level0 = 0;
    item.level1 = 1;
    if (val)
    {
        // write "1" slot
        item.duration0 = OW_DURATION_1_LOW;
        item.duration1 = OW_DURATION_1_HIGH;
    }
    else
    {
        // write "0" slot
        item.duration0 = OW_DURATION_0_LOW;
        item.duration1 = OW_DURATION_0_HIGH;
    }

    return item;
}
//---------------------------------------------------------------------

static owb_status _write_bits(const OneWireBus * bus, uint8_t out, int number_of_bits_to_write)
{
  rmt_item32_t tx_items[MAX_BITS_PER_SLOT + 1] = {0};
  owb_rmt_driver_info * info = info_of_driver(bus);

  if (number_of_bits_to_write > MAX_BITS_PER_SLOT)
  {
      return OWB_STATUS_TOO_MANY_BITS;
  }

  // write requested bits as pattern to TX buffer
  for (int i = 0; i < number_of_bits_to_write; i++)
  {
      tx_items[i] = _encode_write_slot(out & 0x01);
      out >>= 1;
  }

  // end marker
  tx_items[number_of_bits_to_write].level0 = 1;
  tx_items[number_of_bits_to_write].duration0 = 0;

  owb_status status = OWB_STATUS_NOT_SET;

  if (rmt_write_items(info->tx_channel, tx_items, number_of_bits_to_write+1, true) == ESP_OK)
  {
      status = OWB_STATUS_OK;
  }
  else
  {
      status = OWB_STATUS_HW_ERROR;
      ESP_LOGE(TAG, "rmt_write_items() failed");
  }

  return status;
}
//---------------------------------------------------------------------
static rmt_item32_t _encode_read_slot(void)
{
    rmt_item32_t item = {0};

    // construct pattern for a single read time slot
    item.level0    = 0;
    item.duration0 = OW_DURATION_1_LOW;   // shortly force 0
    item.level1    = 1;
    item.duration1 = OW_DURATION_1_HIGH;  // release high and finish slot
    return item;
}
//---------------------------------------------------------------------
static owb_status _read_bits(const OneWireBus * bus, uint8_t *in, int number_of_bits_to_read)
{
  rmt_item32_t tx_items[MAX_BITS_PER_SLOT + 1] = {0};
  uint8_t read_data = 0;
  int res = OWB_STATUS_OK;

  owb_rmt_driver_info *info = info_of_driver(bus);

  if (number_of_bits_to_read > MAX_BITS_PER_SLOT)
  {
      ESP_LOGE(TAG, "_read_bits() OWB_STATUS_TOO_MANY_BITS");
      return OWB_STATUS_TOO_MANY_BITS;
  }

  // generate requested read slots
  for (int i = 0; i < number_of_bits_to_read; i++)
  {
      tx_items[i] = _encode_read_slot();
  }

  // end marker
  tx_items[number_of_bits_to_read].level0 = 1;
  tx_items[number_of_bits_to_read].duration0 = 0;

  onewire_flush_rmt_rx_buf(bus);

  rmt_rx_start(info->rx_channel, true);

  if (rmt_write_items(info->tx_channel, tx_items, number_of_bits_to_read+1, true) == ESP_OK)
  {
    size_t rx_size = 0;
    rmt_item32_t *rx_items = (rmt_item32_t *)xRingbufferReceive(info->rb, &rx_size, 100 / portTICK_PERIOD_MS);

    if (rx_items)
    {
#ifdef OW_DEBUG
      for (int i = 0; i < rx_size / 4; i++)
      {
          ESP_LOGI(TAG, "level: %d, duration %d", rx_items[i].level0, rx_items[i].duration0);
          ESP_LOGI(TAG, "level: %d, duration %d", rx_items[i].level1, rx_items[i].duration1);
      }
#endif
      if (rx_size >= number_of_bits_to_read * sizeof(rmt_item32_t))
      {
        for (int i = 0; i < number_of_bits_to_read; i++)
        {
          read_data >>= 1;
          // parse signal and identify logical bit
          if (rx_items[i].level1 == 1)
          {
            if ((rx_items[i].level0 == 0) && (rx_items[i].duration0 < OW_DURATION_SAMPLE))
            {
                // rising edge occured before 15us -> bit 1
                read_data |= 0x80;
            }
          }
        }
        read_data >>= 8 - number_of_bits_to_read;
      }
      vRingbufferReturnItem(info->rb, (void *)rx_items);
    }
    else
    {
        // time out occurred, this indicates an unconnected / misconfigured bus
        ESP_LOGE(TAG, "rx_items == 0");
        res = OWB_STATUS_HW_ERROR;
    }
  }
  else
  {
      // error in tx channel
      ESP_LOGE(TAG, "Error tx");
      res = OWB_STATUS_HW_ERROR;
  }

  rmt_rx_stop(info->rx_channel);

  *in = read_data;

  return res;
}
//---------------------------------------------------------------------
owb_status owb_read_bit(const OneWireBus * bus, uint8_t * out)
{
  owb_status status = OWB_STATUS_NOT_SET;

  if (!bus || !out)
  {
    status = OWB_STATUS_PARAMETER_NULL;
  }
  else if (!_is_init(bus))
  {
    status = OWB_STATUS_NOT_INITIALIZED;
  }
  else
  {
    bus->driver->read_bits(bus, out, 1);
    ESP_LOGD(TAG, "owb_read_bit: %02x", *out);
    status = OWB_STATUS_OK;
  }
  return status;
}
//---------------------------------------------------------------------
owb_status owb_read_bytes(const OneWireBus * bus, uint8_t * buffer, unsigned int len)
{
    owb_status status = OWB_STATUS_NOT_SET;

    if (!bus || !buffer)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        for (int i = 0; i < len; ++i)
        {
            uint8_t out;
            bus->driver->read_bits(bus, &out, 8);
            buffer[i] = out;
        }

        ESP_LOGD(TAG, "owb_read_bytes, len %d:", len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, buffer, len, ESP_LOG_DEBUG);

        status = OWB_STATUS_OK;
    }

    return status;
}
//---------------------------------------------------------------------
owb_status owb_write_byte(const OneWireBus * bus, uint8_t data)
{
    owb_status status = OWB_STATUS_NOT_SET;

    if (!bus)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        ESP_LOGD(TAG, "owb_write_byte: %02x", data);
        bus->driver->write_bits(bus, data, 8);
        status = OWB_STATUS_OK;
    }

    return status;
}
//---------------------------------------------------------------------
owb_status owb_write_bytes(const OneWireBus * bus, const uint8_t * buffer, size_t len)
{
    owb_status status = OWB_STATUS_NOT_SET;

    if (!bus || !buffer)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        ESP_LOGD(TAG, "owb_write_bytes, len %d:", len);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, buffer, len, ESP_LOG_DEBUG);

        for (int i = 0; i < len; i++)
        {
            bus->driver->write_bits(bus, buffer[i], 8);
        }

        status = OWB_STATUS_OK;
    }

    return status;
}
//---------------------------------------------------------------------
owb_status owb_write_rom_code(const OneWireBus * bus, OneWireBus_ROMCode rom_code)
{
    owb_status status = OWB_STATUS_NOT_SET;

    if (!bus)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        owb_write_bytes(bus, (uint8_t*)&rom_code, sizeof(rom_code));
        status = OWB_STATUS_OK;
    }

    return status;
}
//---------------------------------------------------------------------
static struct owb_driver rmt_function_table =
{
  .name = "owb_rmt",
  .uninitialize = _uninitialize,
  .reset = _reset,
  .write_bits = _write_bits,
  .read_bits = _read_bits
};
//---------------------------------------------------------------------
static owb_status _init(owb_rmt_driver_info *info, gpio_num_t gpio_num,
                        rmt_channel_t tx_channel, rmt_channel_t rx_channel)
{
  owb_status status = OWB_STATUS_HW_ERROR;

  info->bus.driver = &rmt_function_table;
  info->tx_channel = tx_channel;
  info->rx_channel = rx_channel;
  info->gpio = gpio_num;

#ifdef OW_DEBUG
  ESP_LOGI(TAG, "RMT TX channel: %d", info->tx_channel);
  ESP_LOGI(TAG, "RMT RX channel: %d", info->rx_channel);
#endif

  rmt_config_t rmt_tx = {0};
  rmt_tx.channel = info->tx_channel;
  rmt_tx.gpio_num = gpio_num;
  rmt_tx.mem_block_num = 1;
  rmt_tx.clk_div = 80;
  rmt_tx.tx_config.loop_en = false;
  rmt_tx.tx_config.carrier_en = false;
  rmt_tx.tx_config.idle_level = 1;
  rmt_tx.tx_config.idle_output_en = true;
  rmt_tx.rmt_mode = RMT_MODE_TX;
  if (rmt_config(&rmt_tx) == ESP_OK)
  {
    rmt_set_source_clk(info->tx_channel, RMT_BASECLK_APB);  // only APB is supported by IDF 4.2
    if (rmt_driver_install(rmt_tx.channel, 0, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED) == ESP_OK)
    {
      rmt_config_t rmt_rx = {0};
      rmt_rx.channel = info->rx_channel;
      rmt_rx.gpio_num = gpio_num;
      rmt_rx.clk_div = 80;
      rmt_rx.mem_block_num = 1;
      rmt_rx.rmt_mode = RMT_MODE_RX;
      rmt_rx.rx_config.filter_en = true;
      rmt_rx.rx_config.filter_ticks_thresh = 30;
      rmt_rx.rx_config.idle_threshold = OW_DURATION_RX_IDLE;
      if (rmt_config(&rmt_rx) == ESP_OK)
      {
        rmt_set_source_clk(info->rx_channel, RMT_BASECLK_APB);  // only APB is supported by IDF 4.2
        if (rmt_driver_install(rmt_rx.channel, 512, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED) == ESP_OK)
        {
          rmt_get_ringbuf_handle(info->rx_channel, &info->rb);
          status = OWB_STATUS_OK;
        }
        else
        {
          ESP_LOGE(TAG, "failed to install rx driver");
        }
      }
      else
      {
        status = OWB_STATUS_HW_ERROR;
        ESP_LOGE(TAG, "failed to configure rx, uninstalling rmt driver on tx channel");
        rmt_driver_uninstall(rmt_tx.channel);
      }
    }
    else
    {
      ESP_LOGE(TAG, "failed to install tx driver");
    }
  }
  else
  {
    ESP_LOGE(TAG, "failed to configure tx");
  }

  // attach GPIO to previous pin
  if (gpio_num < 32)
  {
      GPIO.enable_w1ts = (0x1 << gpio_num);
  }
  else
  {
      GPIO.enable1_w1ts.data = (0x1 << (gpio_num - 32));
  }

  rmt_set_pin(info->rx_channel, RMT_MODE_RX, gpio_num);
  rmt_set_pin(info->tx_channel, RMT_MODE_TX, gpio_num);

  // force pin direction to input to enable path to RX channel
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[gpio_num]);

  // enable open drain
  GPIO.pin[gpio_num].pad_driver = 1;


  return status;
}
//---------------------------------------------------------------------
owb_status owb_use_parasitic_power(OneWireBus * bus, bool use_parasitic_power)
{
  owb_status status = OWB_STATUS_NOT_SET;

  if (!bus)
  {
    status = OWB_STATUS_PARAMETER_NULL;
  }
  else if (!_is_init(bus))
  {
    status = OWB_STATUS_NOT_INITIALIZED;
  }
  else
  {
    bus->use_parasitic_power = use_parasitic_power;
    ESP_LOGD(TAG, "use_parasitic_power %d", bus->use_parasitic_power);

    status = OWB_STATUS_OK;
  }

  return status;
}
//---------------------------------------------------------------------
owb_status owb_read_rom(const OneWireBus * bus, OneWireBus_ROMCode *rom_code)
{
  owb_status status = OWB_STATUS_NOT_SET;

  memset(rom_code, 0, sizeof(OneWireBus_ROMCode));

  if (!bus || !rom_code)
  {
    status = OWB_STATUS_PARAMETER_NULL;
  }
  else if (!_is_init(bus))
  {
    status = OWB_STATUS_NOT_INITIALIZED;
  }
  else
  {
    bool is_present;
    bus->driver->reset(bus, &is_present);
    if (is_present)
    {
      uint8_t value = OWB_ROM_READ;
      bus->driver->write_bits(bus, value, 8);
      owb_read_bytes(bus, rom_code->bytes, sizeof(OneWireBus_ROMCode));

      if (bus->use_crc)
      {
        if (owb_crc8_bytes(0, rom_code->bytes, sizeof(OneWireBus_ROMCode)) != 0)
        {
          ESP_LOGE(TAG, "CRC failed");
          memset(rom_code->bytes, 0, sizeof(OneWireBus_ROMCode));
          status = OWB_STATUS_CRC_FAILED;
        }
        else
        {
          status = OWB_STATUS_OK;
        }
      }
      else
      {
        status = OWB_STATUS_OK;
      }
      char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
      owb_string_from_rom_code(*rom_code, rom_code_s, sizeof(rom_code_s));
      ESP_LOGD(TAG, "rom_code %s", rom_code_s);
    }
    else
    {
      status = OWB_STATUS_DEVICE_NOT_RESPONDING;
      ESP_LOGE(TAG, "device not responding");
    }
  }

  return status;
}
//---------------------------------------------------------------------
owb_status owb_verify_rom(const OneWireBus * bus, OneWireBus_ROMCode rom_code, bool * is_present)
{
  owb_status status = OWB_STATUS_NOT_SET;
  bool result = false;

  if (!bus || !is_present)
  {
      status = OWB_STATUS_PARAMETER_NULL;
  }
  else if (!_is_init(bus))
  {
      status = OWB_STATUS_NOT_INITIALIZED;
  }
  else
  {
    OneWireBus_SearchState state = {
        .rom_code = rom_code,
        .last_discrepancy = 64,
        .last_device_flag = false,
    };
    bool is_found = false;
    _search(bus, &state, &is_found);

    if (is_found)
    {
      result = true;
      for (int i = 0; i < sizeof(state.rom_code.bytes) && result; ++i)
      {
        result = rom_code.bytes[i] == state.rom_code.bytes[i];
        ESP_LOGD(TAG, "%02x %02x", rom_code.bytes[i], state.rom_code.bytes[i]);
      }
      ESP_LOGD(TAG, "state.last_discrepancy %d, state.last_device_flag %d, is_found %d",
               state.last_discrepancy, state.last_device_flag, is_found);

      ESP_LOGD(TAG, "rom code %sfound", result ? "" : "not ");
      *is_present = result;
      status = OWB_STATUS_OK;
    }
  }

  return status;
}
//---------------------------------------------------------------------
owb_status owb_reset(const OneWireBus * bus, bool * a_device_present)
{
    owb_status status = OWB_STATUS_NOT_SET;

    if (!bus || !a_device_present)
    {
        status = OWB_STATUS_PARAMETER_NULL;
    }
    else if(!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        status = bus->driver->reset(bus, a_device_present);
    }

    return status;
}
//---------------------------------------------------------------------
owb_status owb_set_strong_pullup(const OneWireBus * bus, bool enable)
{
  owb_status status = OWB_STATUS_NOT_SET;

  if (!bus)
  {
    status = OWB_STATUS_PARAMETER_NULL;
  }
  else if (!_is_init(bus))
  {
    status = OWB_STATUS_NOT_INITIALIZED;
  }
  else
  {
    if (bus->use_parasitic_power && bus->strong_pullup_gpio != GPIO_NUM_NC)
    {
      gpio_set_level(bus->strong_pullup_gpio, enable ? 1 : 0);
      ESP_LOGD(TAG, "strong pullup GPIO %d", enable);
    }  // else ignore

    status = OWB_STATUS_OK;
  }

  return status;
}
//---------------------------------------------------------------------
OneWireBus * owb_rmt_initialize(owb_rmt_driver_info * info, gpio_num_t gpio_num,
                                rmt_channel_t tx_channel, rmt_channel_t rx_channel)
{
  ESP_LOGD(TAG, "%s: gpio_num: %d, tx_channel: %d, rx_channel: %d",
             __func__, gpio_num, tx_channel, rx_channel);

  owb_status status = _init(info, gpio_num, tx_channel, rx_channel);
  if (status != OWB_STATUS_OK)
  {
      ESP_LOGE(TAG, "_init() failed with status %d", status);
  }

  info->bus.strong_pullup_gpio = GPIO_NUM_NC;

  return &(info->bus);
}
//---------------------------------------------------------------------
owb_status owb_uninitialize(OneWireBus * bus)
{
    owb_status status = OWB_STATUS_NOT_SET;

    if (!_is_init(bus))
    {
        status = OWB_STATUS_NOT_INITIALIZED;
    }
    else
    {
        bus->driver->uninitialize(bus);
        status = OWB_STATUS_OK;
    }

    return status;
}
//---------------------------------------------------------------------

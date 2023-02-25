#include "temperature.h"
//==============================================================
char *TAG;
//==============================================================
#define MAX_DEVICES          (8)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD        (1000)   // milliseconds
//==============================================================
// шина 1-wire:
OneWireBus * owb;
// 1-wire устройства:
OneWireBus_ROMCode *device_rom_codes;
// DS1820 датчики:
DS18B20_Info *devices;
// количество найденных датчиков:
int num_devices;
// массив с данными устройств, выдаваемый наверх
TEMPERATURE_device *temp_devices;

int temperature_deactivate_devices()
{
  TAG="temperature_deactivate_devices()";
  ESP_LOGI(TAG,"start");
  owb_uninitialize(owb);
  if(device_rom_codes)
  {
    free(device_rom_codes);
    device_rom_codes=NULL;
  }
  if(devices)
  {
    free(devices);
    devices=NULL;
  }
  if(temp_devices)
  {
    free(temp_devices);
    temp_devices=NULL;
  }
  ESP_LOGI(TAG,"end");
  return 0;
}

TEMPERATURE_device * temperature_get_devices(void)
{
  return temp_devices;
}

int temperature_update_device_data(void)
{
  TAG="temperature_update_device_data()";
  ESP_LOGI(TAG,"start");

  if (num_devices > 0)
  {
    ESP_LOGI(TAG,"1");
    TickType_t last_wake_time = xTaskGetTickCount();
    ESP_LOGI(TAG,"2");
    ds18b20_convert_all(owb);
    ESP_LOGI(TAG,"3");
    ds18b20_wait_for_conversion(devices); // devices[0]
    ESP_LOGI(TAG,"4");

    float reading_temp;
    DS18B20_ERROR ds_error;
    ESP_LOGI(TAG,"5");
    for (int i = 0; i < num_devices; i++)
    {
    ESP_LOGI(TAG,"6");
      ds_error = ds18b20_read_temp(devices+i, &reading_temp);
    ESP_LOGI(TAG,"7");
      if (ds_error != DS18B20_OK)
        (temp_devices+i)->errors++;
      else
        (temp_devices+i)->temp=reading_temp;
    }
    vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
  }
  else
  {
    ESP_LOGE(TAG,"No DS18B20 devices detected!");
    ESP_LOGI(TAG,"end");
    return -1;
  }
  ESP_LOGI(TAG,"end");
  return 0;
}

// поиск всех датчиков температуры (DS1820), подключённых к шине и их инициализация:
int temperature_init_devices(void)
{
  int i=0;
  TAG="temperature_init_devices()";
  ESP_LOGI(TAG,"start");

  // инициализация глобальных переменных:
  device_rom_codes = NULL;
  devices = NULL;
  num_devices=0;
  temp_devices=NULL;

  // Create a 1-Wire bus, using the RMT timeslot driver
  owb_rmt_driver_info rmt_driver_info;
  owb = owb_rmt_initialize(&rmt_driver_info, CONFIG_ONE_WIRE_GPIO, RMT_CHANNEL_1, RMT_CHANNEL_0);
  owb_use_crc(owb, true);  // enable CRC check for ROM code
  // Find all connected devices
  printf("Find devices:\n");
  num_devices = 0;
  OneWireBus_SearchState search_state = {0};
  bool found = false;
  owb_search_first(owb, &search_state, &found);
  while (found)
  {
    char rom_code_s[17];
    owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
    printf("  %d : %s\n", num_devices, rom_code_s);
    device_rom_codes = realloc(device_rom_codes,sizeof(OneWireBus_ROMCode)*(num_devices+1));
    if (device_rom_codes == NULL)
    {
      ESP_LOGE(TAG,"error reallocate %d bytes",sizeof(OneWireBus_ROMCode)*(num_devices+1));
      return -1;
    }
    // обнуляем добавленные байты:
    memset(device_rom_codes+sizeof(OneWireBus_ROMCode)*num_devices, 0, sizeof(OneWireBus_ROMCode));
    *(device_rom_codes+num_devices) = search_state.rom_code;
    ++num_devices;
    owb_search_next(owb, &search_state, &found);
  }
  ESP_LOGI(TAG,"Found %d device%s\n", num_devices, num_devices == 1 ? "" : "s");

  // Create DS18B20 devices on the 1-Wire bus
  // создаём массив структур DS1820 датчиков:
  devices = malloc(sizeof(DS18B20_Info)*num_devices);
  if (devices == NULL)
  {
    ESP_LOGE(TAG,"error allocate %d bytes",sizeof(DS18B20_Info)*num_devices);
    return -1;
  }
  // обнуляем выделенную память:
  memset(devices, 0, sizeof(DS18B20_Info)*num_devices);
  // сопоставляем каждому 1-wire устройству - соответствующий DS1820 датчик:
  ESP_LOGI(TAG,"num_devices=%d",num_devices);
  ESP_LOGI(TAG,"sizeof(DS18B20_Info)=%x",sizeof(DS18B20_Info));
  ESP_LOGI(TAG,"devices=%p",(void*)devices);
  ESP_LOGI(TAG,"devices+1=%p",(void*)(devices+1));
  ESP_LOGI(TAG,"devices+2=%p",(void*)(devices+2));
  for (i = 0; i < num_devices; ++i)
  {
    if (num_devices == 1)
    {
        ESP_LOGI(TAG,"Single device optimisations enabled");
        ds18b20_init_solo(devices+i, owb);          // only one device on bus
    }
    else
    {
        ds18b20_init(devices+i, owb, *(device_rom_codes+i)); // associate with bus and device
    }
    ds18b20_use_crc(devices+i, true);           // enable CRC check on all reads
    ds18b20_set_resolution(devices+i, DS18B20_RESOLUTION);
  }

  // Check for parasitic-powered devices
  bool parasitic_power = false;
  ds18b20_check_for_parasite_power(owb, &parasitic_power);

  if (parasitic_power) {
      ESP_LOGW(TAG,"Parasitic-powered devices detected");
  }
  owb_use_parasitic_power(owb, parasitic_power);

  // создаём выходной массив устройств, который будем заполнять данными:
  temp_devices = malloc(sizeof(TEMPERATURE_device)*num_devices);
  if (temp_devices == NULL)
  {
    ESP_LOGE(TAG,"error allocate %d bytes",sizeof(TEMPERATURE_device)*num_devices);
    return -1;
  }
  // обнуляем выделенную память:
  memset(temp_devices, 0, sizeof(TEMPERATURE_device)*num_devices);
  for(i=0; i<num_devices;i++)
  {
    (temp_devices+i)->temp=ERROR_TEMPERATURE;
    // адрес устройства - в строку:
    owb_string_from_rom_code(*(device_rom_codes+i), (temp_devices+i)->device_addr, OWB_ROM_CODE_STRING_LENGTH);
  }
  ESP_LOGI(TAG,"end");
  return num_devices;
}

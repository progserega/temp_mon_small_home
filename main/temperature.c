#include "temperature.h"
//==============================================================
char *TAG;
//==============================================================
#define MAX_DEVICES          (8)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD        (1000)   // milliseconds
//==============================================================

int temperature_deactivate_devices(TEMPERATURE_data *td)
{
  TAG="temperature_deactivate_devices()";
  ESP_LOGI(TAG,"start");
  owb_uninitialize(td->owb);
  if(td->device_rom_codes)
  {
    free(td->device_rom_codes);
  }
  if(td->devices)
  {
    free(td->devices);
  }
  if(td->temp_devices)
  {
    free(td->temp_devices);
  }
  if(td)
  {
    free(td);
  }
  ESP_LOGI(TAG,"end");
  return 0;
}

int temperature_update_device_data(TEMPERATURE_data *td)
{
  TAG="temperature_update_device_data()";
  ESP_LOGI(TAG,"start");
  ESP_LOGD(TAG,"td=%p",(void*)td);

  if (td->num_devices > 0)
  {
    ESP_LOGI(TAG,"1");
    TickType_t last_wake_time = xTaskGetTickCount();
    ESP_LOGI(TAG,"2");
    ds18b20_convert_all(td->owb);
    ESP_LOGI(TAG,"3");
    ds18b20_wait_for_conversion(td->devices); // td->devices[0]
    ESP_LOGI(TAG,"4");

    float reading_temp;
    DS18B20_ERROR ds_error;
    ESP_LOGI(TAG,"5");
    for (int i = 0; i < td->num_devices; i++)
    {
    ESP_LOGI(TAG,"6");
      ds_error = ds18b20_read_temp(td->devices+i, &reading_temp);
    ESP_LOGI(TAG,"7");
      if (ds_error != DS18B20_OK)
        (td->temp_devices+i)->errors++;
      else
        (td->temp_devices+i)->temp=reading_temp;
    }
    vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
  }
  else
  {
    ESP_LOGE(TAG,"No DS18B20 td->devices detected!");
    ESP_LOGI(TAG,"end");
    return -1;
  }
  ESP_LOGI(TAG,"end");
  return 0;
}

// поиск всех датчиков температуры (DS1820), подключённых к шине и их инициализация:
TEMPERATURE_data* temperature_init_devices(void)
{
  int i=0;
  TAG="temperature_init_devices()";
  ESP_LOGI(TAG,"start");

  // основная структура объекта температур:
  TEMPERATURE_data *td = NULL;
  td = malloc(sizeof(TEMPERATURE_data));
  if (td == NULL)
  {
    ESP_LOGE(TAG,"error allocate %d bytes",sizeof(TEMPERATURE_data));
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td, 0, sizeof(TEMPERATURE_data));
  
  // Create a 1-Wire bus, using the RMT timeslot driver
  owb_rmt_driver_info rmt_driver_info;
  td->owb = owb_rmt_initialize(&rmt_driver_info, CONFIG_ONE_WIRE_GPIO, RMT_CHANNEL_1, RMT_CHANNEL_0);
  owb_use_crc(td->owb, true);  // enable CRC check for ROM code
  // Find all connected td->devices
  printf("Find td->devices:\n");
  OneWireBus_SearchState search_state = {0};
  bool found = false;
  owb_search_first(td->owb, &search_state, &found);
  while (found)
  {
    char rom_code_s[17];
    owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
    printf("  %d : %s\n", td->num_devices, rom_code_s);
    td->device_rom_codes = realloc(td->device_rom_codes,sizeof(OneWireBus_ROMCode)*(td->num_devices+1));
    if (td->device_rom_codes == NULL)
    {
      ESP_LOGE(TAG,"error reallocate %d bytes",sizeof(OneWireBus_ROMCode)*(td->num_devices+1));
      temperature_deactivate_devices(td);
      return NULL;
    }
    // обнуляем добавленные байты:
    memset(td->device_rom_codes+sizeof(OneWireBus_ROMCode)*td->num_devices, 0, sizeof(OneWireBus_ROMCode));
    *(td->device_rom_codes+td->num_devices) = search_state.rom_code;
    ++td->num_devices;
    owb_search_next(td->owb, &search_state, &found);
  }
  ESP_LOGI(TAG,"Found %d device%s\n", td->num_devices, td->num_devices == 1 ? "" : "s");

  // Create DS18B20 td->devices on the 1-Wire bus
  // создаём массив структур DS1820 датчиков:
  td->devices = malloc(sizeof(DS18B20_Info)*td->num_devices);
  if (td->devices == NULL)
  {
    ESP_LOGE(TAG,"error allocate %d bytes",sizeof(DS18B20_Info)*td->num_devices);
    temperature_deactivate_devices(td);
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td->devices, 0, sizeof(DS18B20_Info)*td->num_devices);
  // сопоставляем каждому 1-wire устройству - соответствующий DS1820 датчик:
  ESP_LOGI(TAG,"td->num_devices=%d",td->num_devices);
  ESP_LOGI(TAG,"sizeof(DS18B20_Info)=%x",sizeof(DS18B20_Info));
  ESP_LOGI(TAG,"td->devices=%p",(void*)td->devices);
  ESP_LOGI(TAG,"td->devices+1=%p",(void*)(td->devices+1));
  ESP_LOGI(TAG,"td->devices+2=%p",(void*)(td->devices+2));
  for (i = 0; i < td->num_devices; ++i)
  {
    if (td->num_devices == 1)
    {
        ESP_LOGI(TAG,"Single device optimisations enabled");
        ds18b20_init_solo(td->devices+i, td->owb);          // only one device on bus
    }
    else
    {
        ds18b20_init(td->devices+i, td->owb, *(td->device_rom_codes+i)); // associate with bus and device
    }
    ds18b20_use_crc(td->devices+i, true);           // enable CRC check on all reads
    ds18b20_set_resolution(td->devices+i, DS18B20_RESOLUTION);
  }

  // Check for parasitic-powered td->devices
  bool parasitic_power = false;
  ds18b20_check_for_parasite_power(td->owb, &parasitic_power);

  if (parasitic_power) {
      ESP_LOGW(TAG,"Parasitic-powered td->devices detected");
  }
  owb_use_parasitic_power(td->owb, parasitic_power);

  // создаём выходной массив устройств, который будем заполнять данными:
  td->temp_devices = malloc(sizeof(TEMPERATURE_device)*td->num_devices);
  if (td->temp_devices == NULL)
  {
    ESP_LOGE(TAG,"error allocate %d bytes",sizeof(TEMPERATURE_device)*td->num_devices);
    temperature_deactivate_devices(td);
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td->temp_devices, 0, sizeof(TEMPERATURE_device)*td->num_devices);
  for(i=0; i<td->num_devices;i++)
  {
    (td->temp_devices+i)->temp=ERROR_TEMPERATURE;
    // адрес устройства - в строку:
    owb_string_from_rom_code(*(td->device_rom_codes+i), (td->temp_devices+i)->device_addr, OWB_ROM_CODE_STRING_LENGTH);
  }
  ESP_LOGI(TAG,"end");
  temperature_update_device_data(td);
  temperature_update_device_data(td);
  temperature_update_device_data(td);
  return td;
}

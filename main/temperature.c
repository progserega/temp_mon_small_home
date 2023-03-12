#include "temperature.h"
//==============================================================
static char *TAG="temperature";
//==============================================================
#define MAX_DEVICES          (8)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD        (1000)   // milliseconds
//==============================================================

int temperature_deactivate_devices(TEMPERATURE_data *td)
{
  ESP_LOGD(TAG,"(%s:%d): %s(): start",__FILE__,__LINE__,__func__);
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
  ESP_LOGD(TAG,"(%s:%d): %s(): end",__FILE__,__LINE__,__func__);
  return 0;
}

int temperature_update_device_data(TEMPERATURE_data *td)
{
  ESP_LOGD(TAG,"(%s:%d): %s(): start",__FILE__,__LINE__,__func__);
  if (td->num_devices > 0)
  {
    TickType_t last_wake_time = xTaskGetTickCount();
    ds18b20_convert_all(td->owb);
    ds18b20_wait_for_conversion(td->devices); // td->devices[0]
    float reading_temp;
    DS18B20_ERROR ds_error;
    for (int i = 0; i < td->num_devices; i++)
    {
      ds_error = ds18b20_read_temp(td->devices+i, &reading_temp);
      if (ds_error != DS18B20_OK)
        (td->temp_devices+i)->errors++;
      else
        (td->temp_devices+i)->temp=reading_temp;
    }
    vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
  }
  else
  {
    ESP_LOGE(TAG,"(%s:%d): %s(): No DS18B20 td->devices detected!",__FILE__,__LINE__,__func__);
    ESP_LOGD(TAG,"(%s:%d): %s(): end",__FILE__,__LINE__,__func__);
    return -1;
  }
  ESP_LOGD(TAG,"(%s:%d): %s(): end",__FILE__,__LINE__,__func__);
  return 0;
}

// поиск всех датчиков температуры (DS1820), подключённых к шине и их инициализация:
TEMPERATURE_data* temperature_init_devices(void)
{
  int i=0;
  TAG="temperature_init_devices()";
  ESP_LOGD(TAG,"(%s:%d): %s(): start",__FILE__,__LINE__,__func__);

  // основная структура объекта температур:
  TEMPERATURE_data *td = NULL;
  td = malloc(sizeof(TEMPERATURE_data));
  if (td == NULL)
  {
    ESP_LOGE(TAG,"(%s:%d): %s(): error allocate %d bytes",__FILE__,__LINE__,__func__,sizeof(TEMPERATURE_data));
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td, 0, sizeof(TEMPERATURE_data));
  
  // Create a 1-Wire bus, using the RMT timeslot driver
  td->rmt_driver_info = malloc(sizeof(owb_rmt_driver_info));
  if (td->rmt_driver_info == NULL)
  {
    ESP_LOGE(TAG,"(%s:%d): %s(): error allocate %d bytes",__FILE__,__LINE__,__func__,sizeof(owb_rmt_driver_info));
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td->rmt_driver_info, 0, sizeof(owb_rmt_driver_info));
 
  td->owb = owb_rmt_initialize(td->rmt_driver_info, CONFIG_ONE_WIRE_GPIO, RMT_CHANNEL_1, RMT_CHANNEL_0);
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
      ESP_LOGE(TAG,"(%s:%d): %s(): error reallocate %d bytes",__FILE__,__LINE__,__func__,sizeof(OneWireBus_ROMCode)*(td->num_devices+1));
      temperature_deactivate_devices(td);
      return NULL;
    }
    // обнуляем добавленные байты:
    memset(td->device_rom_codes+sizeof(OneWireBus_ROMCode)*td->num_devices, 0, sizeof(OneWireBus_ROMCode));
    *(td->device_rom_codes+td->num_devices) = search_state.rom_code;
    ++td->num_devices;
    owb_search_next(td->owb, &search_state, &found);
  }
  ESP_LOGI(TAG,"(%s:%d): %s(): Found %d device%s\n",__FILE__,__LINE__,__func__, td->num_devices, td->num_devices == 1 ? "" : "s");

  if(td->num_devices==0){
    ESP_LOGW(TAG,"(%s:%d): %s(): Not found DS1820 temperature sensors on 1-wire bus!",__FILE__,__LINE__,__func__);
    return NULL;
  }


  // Create DS18B20 td->devices on the 1-Wire bus
  // создаём массив структур DS1820 датчиков:
  td->devices = malloc(sizeof(DS18B20_Info)*td->num_devices);
  if (td->devices == NULL)
  {
    ESP_LOGE(TAG,"(%s:%d): %s(): error allocate %d bytes",__FILE__,__LINE__,__func__,sizeof(DS18B20_Info)*td->num_devices);
    temperature_deactivate_devices(td);
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td->devices, 0, sizeof(DS18B20_Info)*td->num_devices);
  // сопоставляем каждому 1-wire устройству - соответствующий DS1820 датчик:
  for (i = 0; i < td->num_devices; ++i)
  {
    if (td->num_devices == 1)
    {
        ESP_LOGI(TAG,"(%s:%d): %s(): Single device optimisations enabled",__FILE__,__LINE__,__func__);
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
    ESP_LOGE(TAG,"(%s:%d): %s(): error allocate %d bytes",__FILE__,__LINE__,__func__,sizeof(TEMPERATURE_device)*td->num_devices);
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
  ESP_LOGD(TAG,"(%s:%d): %s(): end",__FILE__,__LINE__,__func__);
  temperature_update_device_data(td);
  temperature_update_device_data(td);
  temperature_update_device_data(td);
  return td;
}

// приведение к нижнему регистру:
char charToLower(char in) {
    if (in <= 'Z' && in >= 'A')
        return in - ('Z' - 'z');
    return in;
}

// регистронезависимое сравнение строк (приведение к нижнему регистру) - 0 - если равны,
// не ноль - если не равны
int strcicmpL(char const *a, char const *b) {
  while (*b) {
    int d = charToLower(*a) - charToLower(*b);
    if (d) {
        return d;
    } 
    a++;
    b++;
  } 
  return charToLower(*a);
}

// прописываем текстовые имена датчикам:
void add_alias_to_temp_devices(TEMPERATURE_data *td)
{
  for(int i=0;i<td->num_devices;i++)
  {
    if(strcicmpL((td->temp_devices+i)->device_addr,"28A83456B513CF9")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Окруж.воздух:");
        sprintf((td->temp_devices+i)->device_name,   "Okruj.vozduh:   ");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"28813EF41E19124")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня,под утепл:");
        sprintf((td->temp_devices+i)->device_name,   "Kuhnya, pesok:  ");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"2878DDDC1E19138")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня, стяжка:  ");
        sprintf((td->temp_devices+i)->device_name,   "Kuhnya, bet pol:");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"28E04079A2193E0")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Юж комн, песок: ");
        sprintf((td->temp_devices+i)->device_name,   "Yujn komn,pesok:");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"2846779A21326")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня, песок:   ");
        sprintf((td->temp_devices+i)->device_name,   "Kuhna, pesok:   ");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"284FEF79A2135E")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Сев комн, песок:");
        sprintf((td->temp_devices+i)->device_name,   "Sev komn, pesok:");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"28C34279A216394")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Сев ком,под утпл:");
        sprintf((td->temp_devices+i)->device_name,   "Sev kom,pesok:  ");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"a63c01b556edfc28")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Улица:");
        sprintf((td->temp_devices+i)->device_name,   "Ulica:          ");
    }
    else if(strcicmpL((td->temp_devices+i)->device_addr,"7d3c01b556705828")==0)
    {
      // датчик припаянный к распред-плате тестового терминала:
      sprintf((td->temp_devices+i)->device_name,     "test name:      ");
    }
  }
}

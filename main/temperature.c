#include "temperature.h"
#include "spiffs.h"
#include "sys_time.h"
//==============================================================
static char *TAG="temperature";
//==============================================================
#define MAX_DEVICES          (8)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD        (1000)   // milliseconds
//==============================================================

int old_hour=-1;
int old_day=-1;

// обновляем статистику по текущим показаниям датчиков:
int temperature_update_device_stat(TEMPERATURE_data *td)
{
  time_t now = 0;
  struct tm timeinfo = { 0 };
  TEMPERATURE_stat_item *cur_stat_item;
  TEMPERATURE_device *dev;
  int x;

  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);

  // проверяем, обновлено ли время - обновлять ли статистику:
  if (!td->time_updated){
    ESP_LOGW(TAG,"%s(%d): time not synced - skip stat update",__func__,__LINE__);
    return -1;
  }

  time(&now);
  localtime_r(&now, &timeinfo);

  for (int i = 0; i < td->num_devices; i++)
  {
    dev = td->temp_devices+i;
    // приводим к более экономной по памяти форме:
    int cur_temp=(int)(dev->temp*1000);

    // обновляем статистику за час:
    cur_stat_item=dev->stat_day+timeinfo.tm_hour;
    if(cur_stat_item->min==ERROR_TEMPERATURE)cur_stat_item->min=cur_temp;
    if(cur_stat_item->max==ERROR_TEMPERATURE)cur_stat_item->max=cur_temp;
    if(cur_temp<cur_stat_item->min)cur_stat_item->min=cur_temp;
    if(cur_temp>cur_stat_item->max)cur_stat_item->max=cur_temp;
    // обновляем статистику за день:
    cur_stat_item=dev->stat_month+timeinfo.tm_mday-1; // -1 т.к. дни начинаются не с 0, а с 1 (в отличч от часов и месяцев - см. структуру tm в /usr/include/x86_64-linux-gnu/bits/types/struct_tm.h)
    if(cur_stat_item->min==ERROR_TEMPERATURE)cur_stat_item->min=cur_temp;
    if(cur_stat_item->max==ERROR_TEMPERATURE)cur_stat_item->max=cur_temp;
    if(cur_temp<cur_stat_item->min)cur_stat_item->min=cur_temp;
    if(cur_temp>cur_stat_item->max)cur_stat_item->max=cur_temp;
    // обновляем статистику за месяц:
    cur_stat_item=dev->stat_year+timeinfo.tm_mon;
    if(cur_stat_item->min==ERROR_TEMPERATURE)cur_stat_item->min=cur_temp;
    if(cur_stat_item->max==ERROR_TEMPERATURE)cur_stat_item->max=cur_temp;
    if(cur_temp<cur_stat_item->min)cur_stat_item->min=cur_temp;
    if(cur_temp>cur_stat_item->max)cur_stat_item->max=cur_temp;

    // пересчитываем текущую позицию за отчётные периоды:
    // за день:
    for(x=0;x<24;x++){
      cur_stat_item=dev->stat_day+x;
      if(cur_stat_item->max != ERROR_TEMPERATURE){
        if(dev->stat_day_max_temp==ERROR_TEMPERATURE)dev->stat_day_max_temp=cur_stat_item->max;
        if(cur_stat_item->max > dev->stat_day_max_temp)dev->stat_day_max_temp=cur_stat_item->max;
      }
      if(cur_stat_item->min != ERROR_TEMPERATURE){
        if(dev->stat_day_min_temp==ERROR_TEMPERATURE)dev->stat_day_min_temp=cur_stat_item->min;
        if(cur_stat_item->min < dev->stat_day_min_temp)dev->stat_day_min_temp=cur_stat_item->min;
      }
    }
    // за месяц:
    for(x=0;x<31;x++){
      cur_stat_item=dev->stat_month+x;
      if(cur_stat_item->max != ERROR_TEMPERATURE){
        if(dev->stat_month_max_temp==ERROR_TEMPERATURE)dev->stat_month_max_temp=cur_stat_item->max;
        if(cur_stat_item->max > dev->stat_month_max_temp)dev->stat_month_max_temp=cur_stat_item->max;
      }
      if(cur_stat_item->min != ERROR_TEMPERATURE){
        if(dev->stat_month_min_temp==ERROR_TEMPERATURE)dev->stat_month_min_temp=cur_stat_item->min;
        if(cur_stat_item->min < dev->stat_month_min_temp)dev->stat_month_min_temp=cur_stat_item->min;
      }
    }
    // за год:
    for(x=0;x<12;x++){
      cur_stat_item=dev->stat_year+x;
      if(cur_stat_item->max != ERROR_TEMPERATURE){
        if(dev->stat_year_max_temp==ERROR_TEMPERATURE)dev->stat_year_max_temp=cur_stat_item->max;
        if(cur_stat_item->max > dev->stat_year_max_temp)dev->stat_year_max_temp=cur_stat_item->max;
      }
      if(cur_stat_item->min != ERROR_TEMPERATURE){
        if(dev->stat_year_min_temp==ERROR_TEMPERATURE)dev->stat_year_min_temp=cur_stat_item->min;
        if(cur_stat_item->min < dev->stat_year_min_temp)dev->stat_year_min_temp=cur_stat_item->min;
      }
    }

    // проверяем, был ли переход на новый месяц. Если да - надо подчистить оставшиеся дни у статистики
    // текущего датчика, если есть
    // т.е. если в прошедшем месяце, например, было 29 дней, а позапрошлом - 31, то показания 30 и 31 не
    // перезатёрлись в прошлом месяце и позапрошломесячные показания двух дней будут влиять на статистику
    if(old_day!=timeinfo.tm_mday && timeinfo.tm_mday == 1){
      // с прошлого запуска изменился месяц, проверяем - нужно ли перетерать последние дни:
      ESP_LOGI(TAG,"start new month - try clear end days stat");
      for(x=old_day;x<31;x++){
        cur_stat_item=dev->stat_month+x-1; // -1 т.к. дни начинаются не с 0, а с 1 (в отличч от часов и месяцев - см. структуру tm в /usr/include/x86_64-linux-gnu/bits/types/struct_tm.h)
        ESP_LOGD(TAG,"clear data at %d day (index=%d)",x,x-1);
        cur_stat_item->max = ERROR_TEMPERATURE;
        cur_stat_item->min = ERROR_TEMPERATURE;
      }
    }
  }
  // проверяем, нужно ли сохранить данные на флешку:
  if(old_hour!=timeinfo.tm_hour){
    // с прошлого запуска изменился час, проверяем периодичность:
    if ((timeinfo.tm_hour+1)%CONFIG_PERIOD_HOURS_SAVE_STAT_TO_FLASH==0){
      ESP_LOGI(TAG,"start save to flash at %d hour (period in config set as: %d)",timeinfo.tm_hour,CONFIG_PERIOD_HOURS_SAVE_STAT_TO_FLASH);
      if(temperature_stat_save_to_flash(td)==-1){
        ESP_LOGE(TAG,"error temperature_save_to_flash()");
      }
    }
  }

  old_hour=timeinfo.tm_hour;
  old_day=timeinfo.tm_mday;
  return 0;
}

int temperature_stat_save_to_flash(TEMPERATURE_data *td)
{
  esp_vfs_spiffs_conf_t conf;
  TEMPERATURE_device *dev;
  char file_name[120];
  int buf_size;
  int32_t stat[62];
  int x;

  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
  // инициируем и монтируем флешку:
  spiffs_init(&conf,td->num_devices*3); // 3 - количество файлов статистики для одного устройства

  if(spiffs_mount(&conf) == -1){
    ESP_LOGE(TAG,"error spiffs_mount()");
    return -1;
  }

  // сохраняем все структуры для всех датчиков:
  for (int i = 0; i < td->num_devices; i++)
  {
    dev = td->temp_devices+i;
    // сохраняем статистику за сутки:
    sprintf(file_name,"%s_day.stat",dev->device_addr);
    // формируем буфер с данными статистики за сутки:
    buf_size=24*2*sizeof(int32_t);
    ESP_LOGD(TAG,"(%d)",__LINE__);
    for(x=0;x<24;x++){
      stat[x*2]=(dev->stat_day+x)->min;
      stat[x*2+1]=(dev->stat_day+x)->max;
      ESP_LOGD(TAG,"hour=%d, min=%d, max=%d",x,stat[x*2],stat[x*2+1]);
    }
    ESP_LOGI(TAG,"write %d bytes to file: %s",buf_size,file_name);
    if(spiffs_save_buf_to_file(file_name,stat,buf_size)){
      ESP_LOGE(TAG,"error spiffs_save_buf_to_file()");
      return -1;
    }

    // сохраняем статистику за месяц:
    sprintf(file_name,"%s_month.stat",dev->device_addr);
    // формируем буфер с данными статистики за сутки:
    buf_size=31*2*sizeof(int32_t);
    for(x=0;x<31;x++){
      stat[x*2]=(dev->stat_month+x)->min;
      stat[x*2+1]=(dev->stat_month+x)->max;
      ESP_LOGD(TAG,"day=%d, min=%d, max=%d",x,stat[x*2],stat[x*2+1]);
    }
    ESP_LOGI(TAG,"write %d bytes to file: %s",buf_size,file_name);
    if(spiffs_save_buf_to_file(file_name,stat,buf_size)){
      ESP_LOGE(TAG,"error spiffs_save_buf_to_file()");
      return -1;
    }

    // сохраняем статистику за год:
    sprintf(file_name,"%s_year.stat",dev->device_addr);
    // формируем буфер с данными статистики за сутки:
    buf_size=12*2*sizeof(int32_t);
    for(x=0;x<12;x++){
      stat[x*2]=(dev->stat_year+x)->min;
      stat[x*2+1]=(dev->stat_year+x)->max;
      ESP_LOGD(TAG,"month=%d, min=%d, max=%d",x,stat[x*2],stat[x*2+1]);
    }
    ESP_LOGI(TAG,"write %d bytes to file: %s",buf_size,file_name);
    if(spiffs_save_buf_to_file(file_name,stat,buf_size)){
      ESP_LOGE(TAG,"error spiffs_save_buf_to_file()");
      return -1;
    }
  }

  if(spiffs_umount(&conf) == -1){
    ESP_LOGE(TAG,"error spiffs_umount()");
    return -1;
  }
  return 0;
}

int temperature_stat_load_from_flash(TEMPERATURE_data *td)
{
  esp_vfs_spiffs_conf_t conf;
  TEMPERATURE_device *dev;
  char file_name[255];
  int32_t *ret, *stat;
  int stat_size;
  int x;

  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
  // инициируем и монтируем флешку:
  spiffs_init(&conf,td->num_devices*3); // 3 - количество файлов статистики для одного устройства
  if(spiffs_mount(&conf) == -1){
    ESP_LOGE(TAG,"error spiffs_mount()");
    return -1;
  }
  // сохраняем все структуры для всех датчиков:
  for (int i = 0; i < td->num_devices; i++)
  {
    dev = td->temp_devices+i;

    // загружаем дневные данные статистики:
    sprintf(file_name,"%s_day.stat",dev->device_addr);
    ret = (int32_t*)spiffs_get_file_as_buf(file_name);
    if (ret == NULL){
      ESP_LOGE(TAG,"error spiffs_get_file_as_buf(%s)",file_name);
    }
    else{
      stat_size=(*ret)/sizeof(int32_t);
      stat=ret+1;
      ESP_LOGI(TAG,"load %d bytes (%d items) stat-data from file: %s",stat_size*sizeof(int32_t),stat_size,file_name);
      for(x=0;x<24;x++){
        if (stat+x*2+1 >= stat+stat_size){
          ESP_LOGW(TAG,"(%d): index (x=%d) over file data - overflow! skip!",__LINE__,x);
          break;
        }
        (dev->stat_day+x)->min = *(stat+x*2);
        (dev->stat_day+x)->max = *(stat+x*2+1);
      }
    }
    free(ret);
 
    // загружаем данные статистики за месяц:
    sprintf(file_name,"%s_month.stat",dev->device_addr);
    ret = (int32_t*)spiffs_get_file_as_buf(file_name);
    if (ret == NULL){
      ESP_LOGE(TAG,"error spiffs_get_file_as_buf(%s)",file_name);
    }
    else{
      stat_size=(*ret)/sizeof(int32_t);
      stat=ret+1;
      ESP_LOGI(TAG,"load %d bytes (%d items) stat-data from file: %s",stat_size*sizeof(int32_t),stat_size,file_name);
      for(x=0;x<31;x++){
        if (stat+x*2+1 >= stat+stat_size){
          ESP_LOGW(TAG,"(%d): index (x=%d) over file data - overflow! skip!",__LINE__,x);
          break;
        }
        (dev->stat_month+x)->min = *(stat+x*2);
        (dev->stat_month+x)->max = *(stat+x*2+1);
      }
    }
    free(ret);
   
    // загружаем данные статистики за год:
    sprintf(file_name,"%s_year.stat",dev->device_addr);
    ret = (int32_t*)spiffs_get_file_as_buf(file_name);
    if (ret == NULL){
      ESP_LOGE(TAG,"error spiffs_get_file_as_buf(%s)",file_name);
    }
    else{
      stat_size=(*ret)/sizeof(int32_t);
      stat=ret+1;
      ESP_LOGI(TAG,"load %d bytes (%d items) stat-data from file: %s",stat_size*sizeof(int32_t),stat_size,file_name);
      for(x=0;x<12;x++){
        if (stat+x*2+1 >= stat+stat_size){
          ESP_LOGW(TAG,"(%d): index (x=%d) over file data - overflow! skip!",__LINE__,x);
          break;
        }
        (dev->stat_year+x)->min = *(stat+x*2);
        (dev->stat_year+x)->max = *(stat+x*2+1);
      }
    }
    free(ret);
  }
  if(spiffs_umount(&conf) == -1){
    ESP_LOGE(TAG,"error spiffs_umount()");
    return -1;
  }
  return 0;
}

int temperature_deactivate_devices(TEMPERATURE_data *td)
{
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
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
  ESP_LOGD(TAG,"%s(%d): end",__func__,__LINE__);
  return 0;
}

int temperature_update_device_data(TEMPERATURE_data *td)
{
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
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
    ESP_LOGE(TAG,"%s(%d): No DS18B20 td->devices detected!",__func__,__LINE__);
    ESP_LOGD(TAG,"%s(%d): end",__func__,__LINE__);
    return -1;
  }
  ESP_LOGD(TAG,"%s(%d): end",__func__,__LINE__);
  return 0;
}

// поиск всех датчиков температуры (DS1820), подключённых к шине и их инициализация:
TEMPERATURE_data* temperature_init_devices(void)
{
  int i=0;
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);

  // основная структура объекта температур:
  TEMPERATURE_data *td = NULL;
  td = malloc(sizeof(TEMPERATURE_data));
  if (td == NULL)
  {
    ESP_LOGE(TAG,"%s(%d): error allocate %d bytes",__func__,__LINE__,sizeof(TEMPERATURE_data));
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td, 0, sizeof(TEMPERATURE_data));
  
  // Create a 1-Wire bus, using the RMT timeslot driver
  td->rmt_driver_info = malloc(sizeof(owb_rmt_driver_info));
  if (td->rmt_driver_info == NULL)
  {
    ESP_LOGE(TAG,"%s(%d): error allocate %d bytes",__func__,__LINE__,sizeof(owb_rmt_driver_info));
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
      ESP_LOGE(TAG,"%s(%d): error reallocate %d bytes",__func__,__LINE__,sizeof(OneWireBus_ROMCode)*(td->num_devices+1));
      temperature_deactivate_devices(td);
      return NULL;
    }
    // обнуляем добавленные байты:
    memset(td->device_rom_codes+sizeof(OneWireBus_ROMCode)*td->num_devices, 0, sizeof(OneWireBus_ROMCode));
    *(td->device_rom_codes+td->num_devices) = search_state.rom_code;
    ++td->num_devices;
    owb_search_next(td->owb, &search_state, &found);
  }
  ESP_LOGI(TAG,"%s(%d): Found %d device%s\n",__func__,__LINE__, td->num_devices, td->num_devices == 1 ? "" : "s");

  if(td->num_devices==0){
    ESP_LOGW(TAG,"%s(%d): Not found DS1820 temperature sensors on 1-wire bus!",__func__,__LINE__);
    return NULL;
  }

  // Create DS18B20 td->devices on the 1-Wire bus
  // создаём массив структур DS1820 датчиков:
  td->devices = malloc(sizeof(DS18B20_Info)*td->num_devices);
  if (td->devices == NULL)
  {
    ESP_LOGE(TAG,"%s(%d): error allocate %d bytes",__func__,__LINE__,sizeof(DS18B20_Info)*td->num_devices);
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
        ESP_LOGI(TAG,"%s(%d): Single device optimisations enabled",__func__,__LINE__);
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
    ESP_LOGE(TAG,"%s(%d): error allocate %d bytes",__func__,__LINE__,sizeof(TEMPERATURE_device)*td->num_devices);
    temperature_deactivate_devices(td);
    return NULL;
  }
  // обнуляем выделенную память:
  memset(td->temp_devices, 0, sizeof(TEMPERATURE_device)*td->num_devices);
  reset_temperature(td);
  for(i=0; i<td->num_devices;i++)
  {
    // адрес устройства - в строку:
    owb_string_from_rom_code(*(td->device_rom_codes+i), (td->temp_devices+i)->device_addr, OWB_ROM_CODE_STRING_LENGTH);
  }
  ESP_LOGD(TAG,"%s(%d): end",__func__,__LINE__);
  temperature_update_device_data(td);
  temperature_update_device_data(td);
  temperature_update_device_data(td);
  return td;
}

void reset_temperature(TEMPERATURE_data *td)
{
  TEMPERATURE_device *dev;
  TEMPERATURE_stat_item *cur_stat_item;
  for(int i=0; i<td->num_devices;i++)
  {
    dev = td->temp_devices+i;
    dev->temp=ERROR_TEMPERATURE;
    dev->stat_day_max_temp=ERROR_TEMPERATURE;
    dev->stat_day_min_temp=ERROR_TEMPERATURE;
    dev->stat_month_max_temp=ERROR_TEMPERATURE;
    dev->stat_month_min_temp=ERROR_TEMPERATURE;
    dev->stat_year_max_temp=ERROR_TEMPERATURE;
    dev->stat_year_min_temp=ERROR_TEMPERATURE;
    // статистические данные:
    for(int x=0;x<24;x++){
      cur_stat_item=dev->stat_day+x;
      cur_stat_item->min=ERROR_TEMPERATURE;
      cur_stat_item->max=ERROR_TEMPERATURE;
    }
    for(int x=0;x<31;x++){
      cur_stat_item=dev->stat_month+x;
      cur_stat_item->min=ERROR_TEMPERATURE;
      cur_stat_item->max=ERROR_TEMPERATURE;
    }
    for(int x=0;x<12;x++){
      cur_stat_item=dev->stat_year+x;
      cur_stat_item->min=ERROR_TEMPERATURE;
      cur_stat_item->max=ERROR_TEMPERATURE;
    }
  }
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
    //if(strcicmpL((td->temp_devices+i)->device_addr,"28A83456B513CF9")==0)
    if(strcicmpL((td->temp_devices+i)->device_addr,"f93c01b55634a828")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Окруж.воздух:");
        sprintf((td->temp_devices+i)->device_name,   "Okruj.vozduh:   ");
    }
    //else if(strcicmpL((td->temp_devices+i)->device_addr,"28813EF41E19124")==0)
    else if(strcicmpL((td->temp_devices+i)->device_addr,"2401191ef43e8128")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня,под утепл:");
        sprintf((td->temp_devices+i)->device_name,   "Kuhnya, pesok:  ");
    }
    //else if(strcicmpL((td->temp_devices+i)->device_addr,"2878DDDC1E19138")==0)
    else if(strcicmpL((td->temp_devices+i)->device_addr,"3801191edcdd7828")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня, стяжка:  ");
        sprintf((td->temp_devices+i)->device_name,   "Kuhnya, bet pol:");
    }
    // TODO
    else if(strcicmpL((td->temp_devices+i)->device_addr,"28E04079A2193E0")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Юж комн, песок: ");
        sprintf((td->temp_devices+i)->device_name,   "Yujn komn,pesok:");
    }
    //else if(strcicmpL((td->temp_devices+i)->device_addr,"2846779A21326")==0)
    else if(strcicmpL((td->temp_devices+i)->device_addr,"260301a279670428")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня, песок:   ");
        sprintf((td->temp_devices+i)->device_name,   "Kuhna, pesok:   ");
    }
    // TODO
    else if(strcicmpL((td->temp_devices+i)->device_addr,"284FEF79A2135E")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Сев комн, песок:");
        sprintf((td->temp_devices+i)->device_name,   "Sev komn, pesok:");
    }
    // TODO
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

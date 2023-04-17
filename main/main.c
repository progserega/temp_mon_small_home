#include "main.h"
//------------------------------------------------
static char *TAG="main";
//------------------------------------------------
// переменные в других объектных файлах:
// очередь обработки нажатий кнопки:
extern xQueueHandle gpio_evt_queue;
// очередь отправки сообщений на экран:
extern xQueueHandle lcd_string_queue;
// семафор включения подсветки:
extern SemaphoreHandle_t lcd_backlight_sem;
// очередь включения подсветки экрана:
extern xQueueHandle lcd_backlight_queue;
// структура для экрана lcd1620:
extern LiquidCrystal_I2C_Data lcd;

//------------------------------------------------

// обработчик прерывания нажатия кнопки:
static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

// обновляем данные по датчикам:
static void update_ds1820_temp_task(void *arg)
{
  int ret;
  for (;;) {
    // обращаемся к общим данным только через семафор:
    ESP_LOGD(TAG,"%s(%d): xSemaphoreTake(temperature_data_sem):1",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
    ESP_LOGI(TAG,"%s(%d): call update",__func__,__LINE__);
    ret=temperature_update_device_data((TEMPERATURE_data *)arg);
    ESP_LOGD(TAG,"%s(%d): xSemaphoreGive(temperature_data_sem):2",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
    if(ret==-1)
    {
      ESP_LOGE(TAG,"%s(%d): temperature_update_device_data()",__func__,__LINE__);
      // перезагружаем устройство:
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      reboot();
    }
    // обновляем статистику по температуре:
    temperature_update_device_stat((TEMPERATURE_data *)arg);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void gpio_init(void)
{
  gpio_config_t io_conf;

  //interrupt of rising edge
  io_conf.intr_type = GPIO_INTR_NEGEDGE; // GPIO_INTR_POSEDGE - положительный перепад, GPIO_INTR_NEGEDGE - отрицательный перепад
  //bit mask of the pins, use GPIO4/5 here
  io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
  //set as input mode
  io_conf.mode = GPIO_MODE_INPUT;
  //enable pull-up mode
  io_conf.pull_up_en = 1;
  io_conf.pull_down_en = 0;
  gpio_config(&io_conf);

  //change gpio intrrupt type for one pin
  //gpio_set_intr_type(CONFIG_BUTTON_GPIO, GPIO_INTR_ANYEDGE);

  //install gpio isr service
  gpio_install_isr_service(0);
  //hook isr handler for specific gpio pin
  gpio_isr_handler_add(CONFIG_BUTTON_GPIO, gpio_isr_handler, (void *) CONFIG_BUTTON_GPIO);
  //hook isr handler for specific gpio pin

}

//------------------------------------------------

void reboot(void)
{
    ESP_LOGW(TAG,"%s(%d): code call reboot()",__func__,__LINE__);
    printf("Restarting now.\n");
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

void init_wifi(void)
{
  //Initialize NVS
   esp_err_t ret;
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ret = nvs_flash_erase();
    ESP_LOGI(TAG, "nvs_flash_erase: 0x%04x", ret);
    ret = nvs_flash_init();
    ESP_LOGI(TAG, "nvs_flash_init: 0x%04x", ret);
  }
  ESP_LOGI(TAG, "nvs_flash_init: 0x%04x", ret);
  ret = esp_netif_init();
  ESP_LOGI(TAG, "esp_netif_init: %d", ret);
  ret = esp_event_loop_create_default();
  ESP_LOGI(TAG, "esp_event_loop_create_default: %d", ret);
  ret = wifi_init_sta();
  ESP_LOGI(TAG, "wifi_init_sta: %d", ret);
}

void lcdPrint(int x,int y,char*str)
{
  qLCDData xLCDData;
  // выводим этапы инициализации на экран:
  sprintf(xLCDData.str,str);
  xLCDData.x_pos = x;
  xLCDData.y_pos = y;
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
}

void app_main(void)
{
  char buf[17];
  esp_err_t ret;
  // устанавливаем уровни логирования для отдельных модулей:
/*  esp_log_level_set("*", ESP_LOG_ERROR);        // set all components to ERROR level
  esp_log_level_set("owb", ESP_LOG_ERROR);
  esp_log_level_set("wifi", ESP_LOG_WARN);      // enable WARN logs from WiFi stack
  esp_log_level_set("dhcpc", ESP_LOG_INFO);     // enable INFO logs from DHCP client
  esp_log_level_set("http", ESP_LOG_DEBUG);     // enable INFO logs from DHCP client
*/
  lcd_string_queue = xQueueCreate(10, sizeof(qLCDData));
  lcd_backlight_queue = xQueueCreate(10, sizeof(qLCDbacklight));
  //create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  lcd_backlight_sem = xSemaphoreCreateBinary();
  // семафор для работы с данными температуры:
  temperature_data_sem = xSemaphoreCreateBinary();
  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive(temperature_data_sem):2",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);

  // инициализация экрана:
  ret = i2c_ini();
  ESP_LOGI(TAG,"%s(%d): i2c_ini: %d",__func__,__LINE__, ret);
  lcd_init(&lcd,126,16,2,8);  // set the LCD address to 0x27 for a 16 chars and 2 line display, 8 - small font, 10 - big font

  // запускаем потоки работы с экраном - после инициализации экрана:
  xTaskCreate(vLCDTask, "vLCDTask", 2048, NULL, 2, NULL);
  xTaskCreate(vLCDTaskBackLight, "vLCDTaskBackLight", 2048, &lcd, 2, NULL);
  // включаем экран на таймаут:
  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(lcd_backlight_sem);

  // выводим этапы инициализации на экран:
  lcdPrint(0,0,"Gpio init...");
  // задержка для отображения:
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  
  // инициализация gpio для кнопки:
  gpio_init();

  // выводим этапы инициализации на экран:
  lcdPrint(0,0,"Scan 1-wire...");
 
  // инициализация датчиков температуры:
  TEMPERATURE_data *td=temperature_init_devices();
  if(td == NULL)
  {
    ESP_LOGE(TAG,"%s(%d): temperature_init_devices()",__func__,__LINE__);
    ESP_LOGI(TAG,"%s(%d): sleep and reboot",__func__,__LINE__);
    // выводим сообщение об ошибке на экран:
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    lcdPrint(0,0,"Found devices:");
    lcdPrint(0,1,"0");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    lcdPrint(0,0,"reboot...       ");
    lcdPrint(0,1,"                ");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    reboot();
  }
  // прописываем имена устройствам:
  add_alias_to_temp_devices(td);

  //start gpio task
  xTaskCreate(gpio_task, "gpio_task", 2048, td, 10, NULL);

  // запускаем поток обновления температуры:
  xTaskCreate(update_ds1820_temp_task, "update_ds1820_temp_task", 2048, td, 2, NULL);

  // сообщаем количество найденных устройств:
  lcdPrint(0,0,"Found devices:");
  ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
  sprintf(buf,"%d",td->num_devices);
  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
  lcdPrint(0,1,buf);
  // задержка для отображения:
  vTaskDelay(3000 / portTICK_PERIOD_MS);
    
  // запускаем поток передачи данных датчиков на экран:
  xTaskCreate(send_ds1820_temp_to_lcd_task, "send_ds1820_temp_to_lcd_task", 2048, td, 2, NULL);

  // запускаем WiFi:
  ESP_LOGI(TAG,"init wifi");
  init_wifi();

  // запускаем http-сервис:
  ESP_LOGI(TAG,"create http_task");
  xTaskCreate(http_task, "http_task", 4096, td, 5, NULL);

  // задержка для подключения к wifi:
  //vTaskDelay(6000 / portTICK_PERIOD_MS);

  // запускаем синхронизацию времени по сети:
  // ждём обновления времени:
  ESP_LOGI(TAG,"init sntp time sync");
  if(esp_wait_sntp_sync(NTP_WAIT_INFINITY)==false){
    ESP_LOGW(TAG,"Failed to update system time within 10s timeout");
  }
  else{
    ESP_LOGI(TAG,"system time updated success.");
    td->time_updated=true;
  }
}

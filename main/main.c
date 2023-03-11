#include "main.h"
//------------------------------------------------
static char *TAG="main";
//------------------------------------------------
typedef struct
{
  unsigned char y_pos;
  unsigned char x_pos;
  char str[17];
} qLCDData;

typedef struct
{
  int timeout;
} qLCDbacklight;

//------------------------------------------------
// очередь отправки сообщений на экран:
xQueueHandle lcd_string_queue = NULL;
// очередь включения подсветки экрана:
xQueueHandle lcd_backlight_queue = NULL;
SemaphoreHandle_t lcd_backlight_sem;
// семафор раздельного обращения к данным температуры:
SemaphoreHandle_t temperature_data_sem;
// очередь обработки нажатий кнопки:
xQueueHandle gpio_evt_queue = NULL;
// текущий отображаемый датчик температуры:
int current_device_index=0;
// отображать ли автоматически сменяемые показания специальным потоком:
bool auto_show_temp=true;
// флаг включенной подсветки:
bool backlight_enabled=false;
esp_timer_handle_t auto_show_timer;

struct LiquidCrystal_I2C_Data lcd;
//------------------------------------------------

// обработчик прерывания нажатия кнопки:
static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void *td)
{
    uint32_t io_num;
    uint32_t val;
    qLCDbacklight xLCDbacklight;
    bool local_backlight_enable_flag=false;
    bool local_auto_show_temp=false;
    qLCDData xLCDData;
    // таймер:
    const esp_timer_create_args_t auto_show_timer_args = {
            .callback = &enableAutoShowTask,
            /* argument specified here will be passed to timer callback function */
           // .arg = NULL,
            .name = "one-shot"
    };
    ESP_ERROR_CHECK(esp_timer_create(&auto_show_timer_args, &auto_show_timer));
    ESP_LOGD(TAG,"(%s:%d): %s(): show esp_timer_dump():",__FILE__,__LINE__,__func__);
    ESP_ERROR_CHECK(esp_timer_dump(stdout));

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            val = gpio_get_level(io_num);
            ESP_LOGI(TAG,"(%s:%d): %s(): GPIO[%d] intr, val: %d\n",__FILE__,__LINE__,__func__, io_num, val);
            // берём только нижний фронт (нажатие):
            if (io_num == CONFIG_BUTTON_GPIO && val == 0 )
            {
              ESP_LOGI(TAG,"(%s:%d): %s(): auto_show_temp=%i, backlight_enabled=%i",__FILE__,__LINE__,__func__,auto_show_temp,backlight_enabled);
              ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
              local_backlight_enable_flag=backlight_enabled;
              local_auto_show_temp = auto_show_temp;
              ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
              if(local_backlight_enable_flag)
              {
                ESP_LOGD(TAG,"(%s:%d): %s(): current_device_index=%d",__FILE__,__LINE__,__func__, current_device_index);
                /* первый раз нажали кнопку - включается подсветка, показания температуры
                всё так же пролистываются автоматически
                второй раз нажали кнопку - показания перестают пролистываться
                третий раз (и последующие) разы нажали кнопку - пролистываются показания по одному
                (на каждое нажатие).
                После того, как подсветка погаснет - режим автоматического пролистывания заново включится.
                */ 
                if(!local_auto_show_temp)
                {
                  ESP_LOGI(TAG,"(%s:%d): %s(): 2: current_device_index=%d",__FILE__,__LINE__,__func__,current_device_index);
                  // уже выключен автоматический показ, значит переключаем на следующий датчик:
                  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
                  current_device_index++;
                  if(current_device_index>=((TEMPERATURE_data*)td)->num_devices)current_device_index=0;

                  ESP_LOGI(TAG,"(%s:%d): %s(): 3: current_device_index=%d",__FILE__,__LINE__,__func__,current_device_index);
                  // посылаем на экран:
                  // первая строка:
                  if(strlen((((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name)!=0)
                  {
                    sprintf(xLCDData.str,"%s",(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name);
                  }
                  else
                  {
                    // если имя пустое - отображаем шестнадцатиричный адрес датчика:
                    sprintf(xLCDData.str,"%s",(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_addr);
                  }
                  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
                  xLCDData.x_pos = 0;
                  xLCDData.y_pos = 0;
                  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  //                vTaskDelay(500 / portTICK_PERIOD_MS);
                  // вторая строка:
                  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
                  int error=(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->errors;
                  if(error>9999)error=9999;
                  sprintf(xLCDData.str,"%2.2f C,err=%d",
                    (((TEMPERATURE_data*)td)->temp_devices+current_device_index)->temp,
                    error
                    );
                  xLCDData.x_pos = 0;
                  xLCDData.y_pos = 1;
                  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
                  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
                }
                ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
                // включаем таймер, который заново включит автопоказ:
                ESP_LOGD(TAG,"(%s:%d): %s(): show esp_timer_dump()",__FILE__,__LINE__,__func__);
                ESP_ERROR_CHECK(esp_timer_dump(stdout));
                // при повторном нажатии не должно опять запускать ещё раз таймер - иначе падает в панику
                if(auto_show_temp){
                  ESP_LOGI(TAG,"(%s:%d): %s(): start auto_show_timer",__FILE__,__LINE__,__func__);
                  ESP_ERROR_CHECK(esp_timer_start_once(auto_show_timer,CONFIG_FREEZE_AUTO_CHANGE_SHOW_TIMEOUT * 1000 ));
                }
                else{
                  // если таймер уже запущен, то перезапускаем его, чтобы таймер отсчитывался от текущего момента:
                  ESP_LOGI(TAG,"(%s:%d): %s(): restart auto_show_timer",__FILE__,__LINE__,__func__);
                  ESP_ERROR_CHECK(esp_timer_stop(auto_show_timer));
                  ESP_ERROR_CHECK(esp_timer_start_once(auto_show_timer,CONFIG_FREEZE_AUTO_CHANGE_SHOW_TIMEOUT * 1000 ));
                }
                auto_show_temp=false;
                ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);

                ESP_LOGI(TAG,"(%s:%d): %s(): auto_show_temp=%i, backlight_enabled=%i",__FILE__,__LINE__,__func__,auto_show_temp,backlight_enabled);
                ESP_LOGI(TAG,"(%s:%d): %s(): 4: current_device_index=%d",__FILE__,__LINE__,__func__,current_device_index);
              }

              //gpio_set_level(GPIO_OUTPUT_IO_BLINK,val);
              // включаем подсветку:
              ESP_LOGI(TAG,"(%s:%d): %s(): enable backlight",__FILE__,__LINE__,__func__);
              //xLCDbacklight.timeout = 8000; // 8000 ms
              //xQueueSendToBack(lcd_backlight_queue, &xLCDbacklight, 0);
              ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(lcd_backlight_sem);
            }
        }
    }
    // сюда никогда не приходим, но показываем как удаляем таймер:
    ESP_ERROR_CHECK(esp_timer_delete(auto_show_timer));
}

// обновляем данные по датчикам:
static void update_ds1820_temp_task(void *arg)
{
  int ret;
  for (;;) {
    // обращаемся к общим данным только через семафор:
    ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake(temperature_data_sem):1",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
    ESP_LOGI(TAG,"(%s:%d): %s(): call update",__FILE__,__LINE__,__func__);
    ret=temperature_update_device_data((TEMPERATURE_data *)arg);
    ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive(temperature_data_sem):2",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
    if(ret==-1)
    {
      ESP_LOGE(TAG,"(%s:%d): %s(): temperature_update_device_data()",__FILE__,__LINE__,__func__);
      // перезагружаем устройство:
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      reboot();
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
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

// в цикле показываем данные датчиков на экране:
static void send_ds1820_temp_to_lcd_task(void *td)
{
  int num_devices;
  qLCDData xLCDData;
  // получаем количество устройств:
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
  num_devices=((TEMPERATURE_data*)td)->num_devices;
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);

  for (;;) {
    // обращаемся к общим данным только через семафор:
    ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
    int show=auto_show_temp;
    ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
    if (show)
    {
      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
      // берём следующий датчик:
      current_device_index++;
      if(current_device_index>=num_devices)current_device_index=0;

      ESP_LOGI("send_ds1820_temp_to_lcd_task","current_device_index=%d",current_device_index);
      ESP_LOGI("send_ds1820_temp_to_lcd_task","send ds1820_temp to lcd");
      ESP_LOGI("send_ds1820_temp_to_lcd_task","device_name len(%s)=%d",
          (((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name,
          strlen((((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name)
        );
      // если имя не пустое - берём имя, если пустое - адрес:
      if(strlen((((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name)!=0)
      {
        sprintf(xLCDData.str,"%s",(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name);
      }
      else
      {
        // если имя пустое - отображаем шестнадцатиричный адрес датчика:
        sprintf(xLCDData.str,"%s",(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_addr);
      }
      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
      //xLCDData.str = buf;
      xLCDData.x_pos = 0;
      xLCDData.y_pos = 0;
      // первая строка:
      ESP_LOGI("send_ds1820_temp_to_lcd_task","send show string: %s",xLCDData.str);
      xQueueSendToBack(lcd_string_queue, &xLCDData, 0);

      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
      int error=(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->errors;
      if(error>9999)error=9999;
      sprintf(xLCDData.str,"%2.2f C,err=%d",
        (((TEMPERATURE_data*)td)->temp_devices+current_device_index)->temp,
        error
        );
      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
      //xLCDData.str = buf;
      xLCDData.x_pos = 0;
      xLCDData.y_pos = 1;
      // вторая строка:
      xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void vLCDTaskBackLight(void* lcd)
{
  BaseType_t xStatus;
  qLCDbacklight xReceivedData;
  for(;;) {
    //xStatus = xQueueReceive(lcd_backlight_queue, &xReceivedData, 10000 /portTICK_RATE_MS);
    ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake(lcd_backlight_sem):1",__FILE__,__LINE__,__func__);
    xStatus = xSemaphoreTake( lcd_backlight_sem, portMAX_DELAY);
    if (xStatus == pdPASS)
    {
      //ESP_LOGI("vLCDTaskBackLight", "enable backlight for %d ms", xReceivedData.timeout);
      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake(temperature_data_sem):2",__FILE__,__LINE__,__func__);xSemaphoreTake( temperature_data_sem,portMAX_DELAY);
      // включаем флаг включения подсветки:
      backlight_enabled=true;
      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive(temperature_data_sem):3",__FILE__,__LINE__,__func__);xSemaphoreGive( temperature_data_sem );
      lcd_setBacklight(lcd,1);
      vTaskDelay(CONFIG_BACKLIGHT_TIMEOUT / portTICK_PERIOD_MS);
      //vTaskDelay(xReceivedData.timeout / portTICK_PERIOD_MS);
      ESP_LOGI(TAG,"(%s:%d): %s(): disable backlight after %d ms",__FILE__,__LINE__,__func__, CONFIG_BACKLIGHT_TIMEOUT);
      lcd_setBacklight(lcd,0);
      // сбрасываем флаги:
      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake(temperature_data_sem):4",__FILE__,__LINE__,__func__);xSemaphoreTake( temperature_data_sem,portMAX_DELAY);
      // выключаем флаг включения подсветки:
      backlight_enabled=false;
      ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive(temperature_data_sem):5",__FILE__,__LINE__,__func__);xSemaphoreGive( temperature_data_sem );
    }
  }
}
void vLCDTask(void* arg)
{
  BaseType_t xStatus;
  qLCDData xReceivedData;
  for(;;) {
    xStatus = xQueueReceive(lcd_string_queue, &xReceivedData, 10000 /portTICK_RATE_MS);
    if (xStatus == pdPASS)
    {
      lcd_setCursor(&lcd,xReceivedData.x_pos,xReceivedData.y_pos);
      lcd_print(&lcd,xReceivedData.str);
      ESP_LOGI("vLCDTask", "set to position %d,%d string: %s", xReceivedData.x_pos, xReceivedData.y_pos, xReceivedData.str);
    }
  }
}

void enableAutoShowTask(void* arg)
{
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake(temperature_data_sem):1",__FILE__,__LINE__,__func__);xSemaphoreTake( temperature_data_sem,portMAX_DELAY);
  // включаем автопролистывание:
  auto_show_temp=true;
  ESP_LOGD(TAG,"(%s:%d): %s(): set auto_show_temp=true",__FILE__,__LINE__,__func__);
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive(temperature_data_sem):2",__FILE__,__LINE__,__func__);xSemaphoreGive( temperature_data_sem );
  // Stop timer the sooner the better
  // останавливать не нужно, т.к. таймер разовый:
  //ESP_ERROR_CHECK(esp_timer_stop(auto_show_timer));
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
  //gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *) GPIO_INPUT_IO_1);

  //remove isr handler for gpio number.
  //gpio_isr_handler_remove(CONFIG_BUTTON_GPIO);
  //hook isr handler for specific gpio pin again
  //gpio_isr_handler_add(CONFIG_BUTTON_GPIO, gpio_isr_handler, (void *) CONFIG_BUTTON_GPIO);

}

//------------------------------------------------

void reboot(void)
{
    ESP_LOGW(TAG,"(%s:%d): %s(): code call reboot()",__FILE__,__LINE__,__func__);
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

void app_main(void)
{
  qLCDData xLCDData;
  qLCDbacklight xLCDbacklight;
  esp_err_t ret;
  // устанавливаем уровни логирования для отдельных модулей:
  esp_log_level_set("owb", ESP_LOG_ERROR);

  lcd_string_queue = xQueueCreate(10, sizeof(qLCDData));
  lcd_backlight_queue = xQueueCreate(10, sizeof(qLCDbacklight));
  //create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  lcd_backlight_sem = xSemaphoreCreateBinary();
  // семафор для работы с данными температуры:
  temperature_data_sem = xSemaphoreCreateBinary();
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive(temperature_data_sem):2",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);

  // инициализация экрана:
  ret = i2c_ini();
  ESP_LOGI(TAG,"(%s:%d): %s(): i2c_ini: %d",__FILE__,__LINE__,__func__, ret);
  lcd_init(&lcd,126,16,2,8);  // set the LCD address to 0x27 for a 16 chars and 2 line display, 8 - small font, 10 - big font

  // запускаем потоки работы с экраном - после инициализации экрана:
  xTaskCreate(vLCDTask, "vLCDTask", 2048, NULL, 2, NULL);
  xTaskCreate(vLCDTaskBackLight, "vLCDTaskBackLight", 2048, &lcd, 2, NULL);
  // включаем экран на таймаут:
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(lcd_backlight_sem);

  // выводим этапы инициализации на экран:
  sprintf(xLCDData.str,"Gpio init...");
  xLCDData.x_pos = 0;
  xLCDData.y_pos = 0;
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  // задержка для отображения:
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  
  // инициализация gpio для кнопки:
  gpio_init();

  // выводим этапы инициализации на экран:
  sprintf(xLCDData.str,"Scan 1-wire...");
  xLCDData.x_pos = 0;
  xLCDData.y_pos = 0;
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
 
  // инициализация датчиков температуры:
  TEMPERATURE_data *td=temperature_init_devices();
  if(td == NULL)
  {
    ESP_LOGE(TAG,"(%s:%d): %s(): temperature_init_devices()",__FILE__,__LINE__,__func__);
    ESP_LOGI(TAG,"(%s:%d): %s(): sleep and reboot",__FILE__,__LINE__,__func__);
    reboot();
  }
  // прописываем имена устройствам:
  add_alias_to_temp_devices(td);

  //start gpio task
  xTaskCreate(gpio_task, "gpio_task", 2048, td, 10, NULL);

  // запускаем поток обновления температуры:
  xTaskCreate(update_ds1820_temp_task, "update_ds1820_temp_task", 2048, td, 2, NULL);

  // сообщаем количество найденных устройств:
  sprintf(xLCDData.str,"Found devices:");
  xLCDData.x_pos = 0;
  xLCDData.y_pos = 0;
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreTake()",__FILE__,__LINE__,__func__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
  sprintf(xLCDData.str,"%d",td->num_devices);
  ESP_LOGD(TAG,"(%s:%d): %s(): xSemaphoreGive()",__FILE__,__LINE__,__func__);xSemaphoreGive(temperature_data_sem);
  xLCDData.x_pos = 0;
  xLCDData.y_pos = 1;
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  // задержка для отображения:
  vTaskDelay(3000 / portTICK_PERIOD_MS);
    
  // запускаем поток передачи данных датчиков на экран:
  xTaskCreate(send_ds1820_temp_to_lcd_task, "send_ds1820_temp_to_lcd_task", 2048, td, 2, NULL);

  // запускаем WiFi:
  init_wifi();

  // запускаем http-сервис:
  xTaskCreate(http_task, "http_task", 4096, td, 5, NULL);
//  xTaskCreate(http_task, "http_task", 4096, NULL, 5, NULL);
}
//------------------------------------------------

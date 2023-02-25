#include "main.h"
//------------------------------------------------
char *TAG;
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

struct LiquidCrystal_I2C_Data lcd;
//------------------------------------------------

// обработчик прерывания нажатия кнопки:
static void gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void *arg)
{
    uint32_t io_num;
    uint32_t val;
    qLCDbacklight xLCDbacklight;

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            val = gpio_get_level(io_num);
            ESP_LOGI("gpio_task", "GPIO[%d] intr, val: %d\n", io_num, val);
            // берём там, где прерывание на оба фронта:
            if (io_num == CONFIG_BUTTON_GPIO)
            {
              //gpio_set_level(GPIO_OUTPUT_IO_BLINK,val);
              // включаем подсветку:
              ESP_LOGI("gpio_task", "enable backlight");
              //xLCDbacklight.timeout = 8000; // 8000 ms
              //xQueueSendToBack(lcd_backlight_queue, &xLCDbacklight, 0);
              xSemaphoreGive(lcd_backlight_sem);
            }
        }
    }
}

// обновляем данные по датчикам:
static void update_ds1820_temp_task(void *arg)
{
  int ret;
  for (;;) {
    // обращаемся к общим данным только через семафор:
    xSemaphoreTake(temperature_data_sem, NULL);
    ESP_LOGI("update_ds1820_temp_task", "call update");
    ret=temperature_update_device_data((TEMPERATURE_data *)arg);
    xSemaphoreGive(temperature_data_sem);
    if(ret==-1)
    {
      ESP_LOGE("update_ds1820_temp_task()", "temperature_update_device_data()");
      // перезагружаем устройство:
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      reboot();
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// прописываем текстовые имена датчикам:
void add_alias_to_temp_devices(TEMPERATURE_data *td)
{
  for(int i=0;i<td->num_devices;i++)
  {
    if(strcmp((td->temp_devices+i)->device_addr,"28A83456B513CF9")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Окруж.воздух:");
        sprintf((td->temp_devices+i)->device_name,   "Okruj.vozduh:");
    }
    else if(strcmp((td->temp_devices+i)->device_addr,"28813EF41E19124")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня,под утепл:");
        sprintf((td->temp_devices+i)->device_name,   "Kuhnya, pesok:  ");
    }
    else if(strcmp((td->temp_devices+i)->device_addr,"2878DDDC1E19138")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня, стяжка:  ");
        sprintf((td->temp_devices+i)->device_name,   "Kuhnya, bet pol:");
    }
    else if(strcmp((td->temp_devices+i)->device_addr,"28E04079A2193E0")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Юж комн, песок:  ");
        sprintf((td->temp_devices+i)->device_name,   "Yujn komn,pesok:");
    }
    else if(strcmp((td->temp_devices+i)->device_addr,"2846779A21326")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Кухня, песок:  ");
        sprintf((td->temp_devices+i)->device_name,"Kuhna, pesok:  ");
    }
    else if(strcmp((td->temp_devices+i)->device_addr,"284FEF79A2135E")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Сев комн, песок:");
        sprintf((td->temp_devices+i)->device_name,   "Sev komn, pesok:");
    }
    else if(strcmp((td->temp_devices+i)->device_addr,"28C34279A216394")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Сев ком,под утпл:");
        sprintf((td->temp_devices+i)->device_name,   "Sev kom,pesok:");
    }
    else if(strcmp((td->temp_devices+i)->device_addr,"28FCED56B513CA6")==0)
    {
        //sprintf((td->temp_devices+i)->device_name,L"Улица:");
        sprintf((td->temp_devices+i)->device_name,   "Ulica:");
    }
  }
}

// в цикле показываем данные датчиков на экране:
static void send_ds1820_temp_to_lcd_task(void *td)
{
  int num_devices;
  int current_device_index=0;
  qLCDData xLCDData;
  // получаем количество устройств:
  xSemaphoreTake(temperature_data_sem, NULL);
  num_devices=((TEMPERATURE_data*)td)->num_devices;
  xSemaphoreGive(temperature_data_sem);

  for (;;) {
    // обращаемся к общим данным только через семафор:
    xSemaphoreTake(temperature_data_sem, NULL);
    ESP_LOGI("send_ds1820_temp_to_lcd_task","send ds1820_temp to lcd");
    ESP_LOGI("send_ds1820_temp_to_lcd_task","len(%s)=%d",
      (((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name,
      strlen((((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name)
    );
    if(strlen((((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name)!=0)
    {
      sprintf(xLCDData.str,"%s",(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_name);
    }
    else
    {
      // если имя пустое - отображаем шестнадцатиричный адрес датчика:
      sprintf(xLCDData.str,"%s",(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->device_addr);
    }
    xSemaphoreGive(temperature_data_sem);
    //xLCDData.str = buf;
    xLCDData.x_pos = 0;
    xLCDData.y_pos = 0;
    // первая строка:
    xQueueSendToBack(lcd_string_queue, &xLCDData, 0);

    xSemaphoreTake(temperature_data_sem, NULL);
    int error=(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->errors;
    if(error>9999)error=9999;
    sprintf(xLCDData.str,"%2.2f C,err=%d",
      (((TEMPERATURE_data*)td)->temp_devices+current_device_index)->temp,
      error
      );
    xSemaphoreGive(temperature_data_sem);
    //xLCDData.str = buf;
    xLCDData.x_pos = 0;
    xLCDData.y_pos = 1;
    // вторая строка:
    xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    current_device_index++;
    if(current_device_index>=num_devices)current_device_index=0;
  }
}

void vLCDTaskBackLight(void* arg)
{
  BaseType_t xStatus;
  qLCDbacklight xReceivedData;
  for(;;) {
    //xStatus = xQueueReceive(lcd_backlight_queue, &xReceivedData, 10000 /portTICK_RATE_MS);
    xStatus = xSemaphoreTake( lcd_backlight_sem, portMAX_DELAY);
    if (xStatus == pdPASS)
    {
      //ESP_LOGI("vLCDTaskBackLight", "enable backlight for %d ms", xReceivedData.timeout);
      lcd_setBacklight(&lcd,1);
      vTaskDelay(CONFIG_BACKLIGHT_TIMEOUT / portTICK_PERIOD_MS);
      //vTaskDelay(xReceivedData.timeout / portTICK_PERIOD_MS);
      //ESP_LOGI("vLCDTaskBackLight", "disable backlight after %d ms", xReceivedData.timeout);
      lcd_setBacklight(&lcd,0);
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
      //LCD_SetPos(xReceivedData.x_pos,xReceivedData.y_pos);
      //LCD_String(xReceivedData.str);
      lcd_setCursor(&lcd,xReceivedData.x_pos,xReceivedData.y_pos);
      lcd_print(&lcd,xReceivedData.str);

      ESP_LOGI("vLCDTask", "set to position %d,%d string: %s", xReceivedData.x_pos, xReceivedData.y_pos, xReceivedData.str);
    }
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
  //gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *) GPIO_INPUT_IO_1);

  //remove isr handler for gpio number.
  //gpio_isr_handler_remove(CONFIG_BUTTON_GPIO);
  //hook isr handler for specific gpio pin again
  //gpio_isr_handler_add(CONFIG_BUTTON_GPIO, gpio_isr_handler, (void *) CONFIG_BUTTON_GPIO);

}

//------------------------------------------------

void reboot(void)
{
    ESP_LOGW("reboot()","code call reboot()");
    printf("Restarting now.\n");
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

void app_main(void)
{
  TAG="app_main";
  qLCDData xLCDData;
  qLCDbacklight xLCDbacklight;
  esp_err_t ret;

  lcd_string_queue = xQueueCreate(10, sizeof(qLCDData));
  lcd_backlight_queue = xQueueCreate(10, sizeof(qLCDbacklight));
  //create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  lcd_backlight_sem = xSemaphoreCreateBinary();
  // семафор для работы с данными температуры:
  temperature_data_sem = xSemaphoreCreateBinary();

  //start gpio task
  xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
  // инициализация экрана:
  ret = i2c_ini();
  // инициализация gpio для кнопки:
  gpio_init();
  // инициализация датчиков температуры:
  TEMPERATURE_data *td=temperature_init_devices();
  if(td == NULL)
  {
    ESP_LOGE(TAG,"temperature_init_devices()");
    ESP_LOGI(TAG,"sleep and reboot");
    reboot();
  }
  // прописываем имена устройствам:
  add_alias_to_temp_devices(td);

  ESP_LOGI(TAG, "i2c_ini: %d", ret);
  lcd_init(&lcd,126,16,2,8);  // set the LCD address to 0x27 for a 16 chars and 2 line display, 8 - small font, 10 - big font

  // запускаем поток обновления температуры:
  xTaskCreate(update_ds1820_temp_task, "update_ds1820_temp_task", 2048, td, 2, NULL);
  // запускаем потоки работы с экраном - после инициализации экрана:
  xTaskCreate(vLCDTask, "vLCDTask", 2048, NULL, 2, NULL);
  xTaskCreate(vLCDTaskBackLight, "vLCDTaskBackLight", 2048, NULL, 2, NULL);

  // сообщаем количество найденных устройств:
  sprintf(xLCDData.str,"Found devices:");
  xLCDData.x_pos = 0;
  xLCDData.y_pos = 0;
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  xSemaphoreTake(temperature_data_sem, NULL);
  sprintf(xLCDData.str,"%d",td->num_devices);
  xSemaphoreGive(temperature_data_sem);
  xLCDData.x_pos = 0;
  xLCDData.y_pos = 1;
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  // задержка для отображения:
  vTaskDelay(3000 / portTICK_PERIOD_MS);
    
  // запускаем поток передачи данных датчиков на экран:
  xTaskCreate(send_ds1820_temp_to_lcd_task, "send_ds1820_temp_to_lcd_task", 2048, td, 2, NULL);

  // включаем экран на таймаут:
  //xLCDbacklight.timeout = 8000; // 8000 ms
  //xQueueSendToBack(lcd_backlight_queue, &xLCDbacklight, 0);
  xSemaphoreGive(lcd_backlight_sem);
/*
  //LCD_ini(126); // 0x4E - для 4-х строчного дисплея LCD2004A, 126 - для LCD1602A (младшие три бита адреса можно задать перемычками A0,A1,A2 на i2c плате - по умолчанию они подтянуты к 1б но можно замкнуть на землю)
  vTaskDelay(100 / portTICK_PERIOD_MS);
  xLCDData.str = str01;
  for(i=0;i<2;i++){
    xLCDData.x_pos = i*3;
    xLCDData.y_pos = i;
    sprintf(str01,"String %d",i+1);
    xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  }
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  //lcd_clear(&lcd);
  xLCDData.x_pos = 9;
  xLCDData.y_pos = 1;
  xLCDData.str = "        ";
  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  i = 0;
  xLCDData.str = str01;
  while (1) {
    i++;
    if(i>65534) i=0;
    sprintf(str01,"%5d",i);
    xLCDData.x_pos = 11;
    xLCDData.y_pos = 1;
    xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
*/
}
//------------------------------------------------

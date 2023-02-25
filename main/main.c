#include "main.h"
//------------------------------------------------
char *TAG;
//------------------------------------------------
typedef struct
{
  unsigned char y_pos;
  unsigned char x_pos;
  char *str;
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
              xSemaphoreGiveFromISR(lcd_backlight_sem, NULL);
            }
        }
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
    printf("Restarting now.\n");
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

void app_main(void)
{
  TAG="app_main";
  uint16_t i=0;
  char str01[10];
  esp_err_t ret;
  qLCDData xLCDData;
  qLCDbacklight xLCDbacklight;
  lcd_string_queue = xQueueCreate(10, sizeof(qLCDData));
  lcd_backlight_queue = xQueueCreate(10, sizeof(qLCDbacklight));
  //create a queue to handle gpio event from isr
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  lcd_backlight_sem = xSemaphoreCreateBinary();

  xTaskCreate(vLCDTask, "vLCDTask", 2048, NULL, 2, NULL);
  xTaskCreate(vLCDTaskBackLight, "vLCDTaskBackLight", 2048, NULL, 2, NULL);
  //start gpio task
  xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
  // инициализация экрана:
  ret = i2c_ini();
  // инициализация gpio для кнопки:
  gpio_init();
  // инициализация датчиков температуры:
  int num_tem_devices=temperature_init_devices();
  if(num_tem_devices == -1)
  {
    ESP_LOGE(TAG,"temperature_init_devices()");
    temperature_deactivate_devices();
    ESP_LOGI(TAG,"sleep and reboot");
    reboot();
  }
  TEMPERATURE_device *temp_devices = temperature_get_devices();
  while(1)
  {
    if (temperature_update_device_data()==-1)
    {
      ESP_LOGE(TAG,"temperature_update_device_data()");
      temperature_deactivate_devices();
      ESP_LOGI(TAG,"sleep and reboot");
      reboot();
    }
    for(int i=0;i<num_tem_devices;i++)
    {
      printf("\naddr=%s, errors=%i, temp=%f",(temp_devices+i)->device_addr,(temp_devices+i)->errors,(temp_devices+i)->temp);
    }
    vTaskDelay(100000 / portTICK_PERIOD_MS);
  }

  ESP_LOGI(TAG, "i2c_ini: %d", ret);
  lcd_init(&lcd,126,16,2,8);  // set the LCD address to 0x27 for a 16 chars and 2 line display, 8 - small font, 10 - big font

  // включаем экран на таймаут:
  //xLCDbacklight.timeout = 8000; // 8000 ms
  //xQueueSendToBack(lcd_backlight_queue, &xLCDbacklight, 0);
  xSemaphoreGiveFromISR(lcd_backlight_sem, NULL);

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
}
//------------------------------------------------

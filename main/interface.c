#include "interface.h"
//------------------------------------------------
static char *TAG="interface";
//------------------------------------------------

//------------------------------------------------
// очередь отправки сообщений на экран:
xQueueHandle lcd_string_queue = NULL;
// очередь включения подсветки экрана:
xQueueHandle lcd_backlight_queue = NULL;
// семафор включения подсветки:
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
// структура для экрана lcd1620:
LiquidCrystal_I2C_Data lcd;
//------------------------------------------------

void gpio_task(void *td)
{
    uint32_t io_num;
    uint32_t val;
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
    //ESP_LOGD(TAG,"%s(%d): show esp_timer_dump():",__func__,__LINE__);
    //ESP_ERROR_CHECK(esp_timer_dump(stdout));

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            val = gpio_get_level(io_num);
            ESP_LOGI(TAG,"%s(%d): GPIO[%d] intr, val: %d\n",__func__,__LINE__, io_num, val);
            // берём только нижний фронт (нажатие):
            if (io_num == CONFIG_BUTTON_GPIO && val == 0 )
            {
              ESP_LOGI(TAG,"%s(%d): auto_show_temp=%i, backlight_enabled=%i",__func__,__LINE__,auto_show_temp,backlight_enabled);
              ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
              local_backlight_enable_flag=backlight_enabled;
              local_auto_show_temp = auto_show_temp;
              ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
              if(local_backlight_enable_flag)
              {
                ESP_LOGD(TAG,"%s(%d): current_device_index=%d",__func__,__LINE__, current_device_index);
                /* первый раз нажали кнопку - включается подсветка, показания температуры
                всё так же пролистываются автоматически
                второй раз нажали кнопку - показания перестают пролистываться
                третий раз (и последующие) разы нажали кнопку - пролистываются показания по одному
                (на каждое нажатие).
                После того, как подсветка погаснет - режим автоматического пролистывания заново включится.
                */ 
                if(!local_auto_show_temp)
                {
                  ESP_LOGI(TAG,"%s(%d): 2: current_device_index=%d",__func__,__LINE__,current_device_index);
                  // уже выключен автоматический показ, значит переключаем на следующий датчик:
                  ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
                  current_device_index++;
                  if(current_device_index>=((TEMPERATURE_data*)td)->num_devices)current_device_index=0;

                  ESP_LOGI(TAG,"%s(%d): 3: current_device_index=%d",__func__,__LINE__,current_device_index);
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
                  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
                  xLCDData.x_pos = 0;
                  xLCDData.y_pos = 0;
                  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
  //                vTaskDelay(500 / portTICK_PERIOD_MS);
                  // вторая строка:
                  ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
                  int error=(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->errors;
                  if(error>9999)error=9999;
                  sprintf(xLCDData.str,"%2.2f C,err=%d",
                    (((TEMPERATURE_data*)td)->temp_devices+current_device_index)->temp,
                    error
                    );
                  xLCDData.x_pos = 0;
                  xLCDData.y_pos = 1;
                  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
                  xQueueSendToBack(lcd_string_queue, &xLCDData, 0);
                }
                ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
                // включаем таймер, который заново включит автопоказ:
                ESP_LOGD(TAG,"%s(%d): show esp_timer_dump()",__func__,__LINE__);
                //ESP_ERROR_CHECK(esp_timer_dump(stdout));
                // при повторном нажатии не должно опять запускать ещё раз таймер - иначе падает в панику
                if(auto_show_temp){
                  ESP_LOGI(TAG,"%s(%d): start auto_show_timer",__func__,__LINE__);
                  ESP_ERROR_CHECK(esp_timer_start_once(auto_show_timer,CONFIG_FREEZE_AUTO_CHANGE_SHOW_TIMEOUT * 1000 ));
                }
                else{
                  // если таймер уже запущен, то перезапускаем его, чтобы таймер отсчитывался от текущего момента:
                  ESP_LOGI(TAG,"%s(%d): restart auto_show_timer",__func__,__LINE__);
                  ESP_ERROR_CHECK(esp_timer_stop(auto_show_timer));
                  ESP_ERROR_CHECK(esp_timer_start_once(auto_show_timer,CONFIG_FREEZE_AUTO_CHANGE_SHOW_TIMEOUT * 1000 ));
                }
                auto_show_temp=false;
                ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);

                ESP_LOGI(TAG,"%s(%d): auto_show_temp=%i, backlight_enabled=%i",__func__,__LINE__,auto_show_temp,backlight_enabled);
                ESP_LOGI(TAG,"%s(%d): 4: current_device_index=%d",__func__,__LINE__,current_device_index);
              }

              //gpio_set_level(GPIO_OUTPUT_IO_BLINK,val);
              // включаем подсветку:
              ESP_LOGI(TAG,"%s(%d): enable backlight",__func__,__LINE__);
              xSemaphoreGive(lcd_backlight_sem);
            }
        }
    }
    // сюда никогда не приходим, но показываем как удаляем таймер:
    ESP_ERROR_CHECK(esp_timer_delete(auto_show_timer));
}

// в цикле показываем данные датчиков на экране:
void send_ds1820_temp_to_lcd_task(void *td)
{
  int num_devices;
  qLCDData xLCDData;
  // получаем количество устройств:
  ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
  num_devices=((TEMPERATURE_data*)td)->num_devices;
  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);

  for (;;) {
    // обращаемся к общим данным только через семафор:
    ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
    int show=auto_show_temp;
    ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
    if (show)
    {
      ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
      // берём следующий датчик:
      current_device_index++;
      if(current_device_index>=num_devices)current_device_index=0;

      ESP_LOGD(TAG,"%s(%d): send ds1820_temp to lcd",__func__,__LINE__);
      ESP_LOGD(TAG,"%s(%d): current_device_index=%d",__func__,__LINE__,current_device_index);
      ESP_LOGD(TAG,"%s(%d): device_name len(%s)=%d",__func__,__LINE__,
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
      ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
      //xLCDData.str = buf;
      xLCDData.x_pos = 0;
      xLCDData.y_pos = 0;
      // первая строка:
      ESP_LOGD(TAG,"%s(%d): send show string: %s",__func__,__LINE__,xLCDData.str);
      xQueueSendToBack(lcd_string_queue, &xLCDData, 0);

      ESP_LOGD(TAG,"%s(%d): xSemaphoreTake()",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
      int error=(((TEMPERATURE_data*)td)->temp_devices+current_device_index)->errors;
      if(error>9999)error=9999;
      sprintf(xLCDData.str,"%2.2f C,err=%d",
        (((TEMPERATURE_data*)td)->temp_devices+current_device_index)->temp,
        error
        );
      ESP_LOGD(TAG,"%s(%d): xSemaphoreGive()",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
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
  for(;;) {
    ESP_LOGD(TAG,"%s(%d): xSemaphoreTake(lcd_backlight_sem):1",__func__,__LINE__);
    xStatus = xSemaphoreTake( lcd_backlight_sem, portMAX_DELAY);
    if (xStatus == pdPASS)
    {
      ESP_LOGD(TAG,"%s(%d): xSemaphoreTake(temperature_data_sem):2",__func__,__LINE__);xSemaphoreTake( temperature_data_sem,portMAX_DELAY);
      // включаем флаг включения подсветки:
      backlight_enabled=true;
      ESP_LOGD(TAG,"%s(%d): xSemaphoreGive(temperature_data_sem):3",__func__,__LINE__);xSemaphoreGive( temperature_data_sem );
      lcd_setBacklight(lcd,1);
      vTaskDelay(CONFIG_BACKLIGHT_TIMEOUT / portTICK_PERIOD_MS);
      ESP_LOGI(TAG,"%s(%d): disable backlight after %d ms",__func__,__LINE__, CONFIG_BACKLIGHT_TIMEOUT);
      lcd_setBacklight(lcd,0);
      // сбрасываем флаги:
      ESP_LOGD(TAG,"%s(%d): xSemaphoreTake(temperature_data_sem):4",__func__,__LINE__);xSemaphoreTake( temperature_data_sem,portMAX_DELAY);
      // выключаем флаг включения подсветки:
      backlight_enabled=false;
      ESP_LOGD(TAG,"%s(%d): xSemaphoreGive(temperature_data_sem):5",__func__,__LINE__);xSemaphoreGive( temperature_data_sem );
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
      ESP_LOGD(TAG,"%s(%d): set to position %d,%d string: %s",__func__,__LINE__, xReceivedData.x_pos, xReceivedData.y_pos, xReceivedData.str);
    }
  }
}

void enableAutoShowTask(void* arg)
{
  ESP_LOGD(TAG,"%s(%d): xSemaphoreTake(temperature_data_sem):1",__func__,__LINE__);xSemaphoreTake( temperature_data_sem,portMAX_DELAY);
  // включаем автопролистывание:
  auto_show_temp=true;
  ESP_LOGD(TAG,"%s(%d): set auto_show_temp=true",__func__,__LINE__);
  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive(temperature_data_sem):2",__func__,__LINE__);xSemaphoreGive( temperature_data_sem );
  // Stop timer the sooner the better
  // останавливать не нужно, т.к. таймер разовый:
  //ESP_ERROR_CHECK(esp_timer_stop(auto_show_timer));
}


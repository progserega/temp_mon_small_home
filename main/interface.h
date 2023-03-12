#ifndef INTERFACE_H_
#define INTERFACE_H_
//---------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "i2c_user.h"
#include "LiquidCrystal_I2C.h"
#include "temperature.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_system.h"

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
//---------------------------------------------------------------------

// кнопка GPIO14 (D5) - замыкает на землю:
// задаётся через глобальный menucofig
//#define CONFIG_BUTTON_GPIO     14
//#define CONFIG_BACKLIGHT_TIMEOUT=600000

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

#define GPIO_INPUT_PIN_SEL  (1ULL<<CONFIG_BUTTON_GPIO)

void enableAutoShowTask(void* arg);
void gpio_task(void *td);
void send_ds1820_temp_to_lcd_task(void *td);
void vLCDTaskBackLight(void* lcd);
void vLCDTask(void* arg);
void enableAutoShowTask(void* arg);

#endif /* INTERFACE_H_ */

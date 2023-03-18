#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_
//---------------------------------------------------------------------
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
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
#include "interface.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "http.h"
#include "esp_err.h"
#include "esp_system.h"
#include "sys_time.h"
#include "esp_log.h"
//---------------------------------------------------------------------

// кнопка GPIO14 (D5) - замыкает на землю:
// задаётся через глобальный menucofig
//#define CONFIG_BUTTON_GPIO     14
//#define CONFIG_BACKLIGHT_TIMEOUT=600000


#define GPIO_INPUT_PIN_SEL  (1ULL<<CONFIG_BUTTON_GPIO)

void reboot(void);
void enableAutoShowTask(void* arg);
void init_wifi(void);

#endif /* MAIN_MAIN_H_ */

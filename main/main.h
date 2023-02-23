#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_
//---------------------------------------------------------------------
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "i2c_user.h"
#include "LiquidCrystal_I2C.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
//---------------------------------------------------------------------

// кнопка GPIO14 (D5) - замыкает на землю:
// задаётся через глобальный menucofig
//#define CONFIG_BUTTON_GPIO     14
//#define CONFIG_BACKLIGHT_TIMEOUT=600000

#define GPIO_INPUT_PIN_SEL  (1ULL<<CONFIG_BUTTON_GPIO)


#endif /* MAIN_MAIN_H_ */

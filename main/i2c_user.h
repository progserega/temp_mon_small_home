#ifndef MAIN_I2C_USER_H_
#define MAIN_I2C_USER_H_
//---------------------------------------------------------------------
#include <stdint.h>
#include "sdkconfig.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include <unistd.h>
//---------------------------------------------------------------------
// задаются через menuconfig:
//#define CONFIG_SCL_GPIO GPIO_NUM_5
//#define CONFIG_SDA_GPIO GPIO_NUM_4
esp_err_t i2c_ini(void);
void I2C_SendByteByADDR(uint8_t c,uint8_t addr);
//---------------------------------------------------------------------
#endif /* MAIN_I2C_USER_H_ */

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "main.c owb.c ds18b20.c i2c_user.c LiquidCrystal_I2C.c temperature.c http.c wifi.c interface.c sys_time.c spiffs.c")
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()

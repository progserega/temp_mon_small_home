# Описание

# Зависисмости
https://github.com/progserega/LiquidCrystal_I2C_rtos
i2c_user.c ds18b20.c owb.c - взяты с https://narodstream.ru/esp32-urok-40-rmt-ds18b20-inicializaciya/


# config
1. main/CMakeLists.txt - список исходников
2. main/Kconfig.projbuild - опции для menuconfig, чтобы задать свои конфиги (например пины через menuconfig)
```
idf.py menuconfig
```
# buid
```
idf.py build
```
# flash
```
idf.py flash
```
# monitor
```
idf.py monitor
```

# Описание
Маленький "умный дом" на ESP32 с маленьким же экранчиком LCD1602, который показывает температуру с датчиков DS1820, сохраняя значения на встроенной энергонезависимой памяти ESP32. А так же выдаёт данные в виде json-а по http по wifi.

# Зависисмости
https://github.com/progserega/LiquidCrystal_I2C_rtos

i2c_user.c ds18b20.c owb.c - взяты с https://narodstream.ru/esp32-urok-40-rmt-ds18b20-inicializaciya/


# Настройка
## config
1. main/CMakeLists.txt - список исходников
2. main/Kconfig.projbuild - опции для menuconfig, чтобы задать свои конфиги (например пины через menuconfig)
```
idf.py menuconfig
```
## buid
```
idf.py build
```
## flash
```
idf.py flash
```
## monitor
```
idf.py monitor
```

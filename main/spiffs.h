#ifndef SPIFFS_H_
#define SPIFFS_H_
//---------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

// инициализация структуры spiffs:
void spiffs_init(esp_vfs_spiffs_conf_t *conf, int max_files);
// монтирование флешки (при необходимости - форматирование её, если она не отформатирована):
int spiffs_mount(esp_vfs_spiffs_conf_t *conf);
// вывод информации в консоль по примонтировааному разделу:
int spiffs_info(esp_vfs_spiffs_conf_t *conf);
// отмонтирование флешки:
int spiffs_umount(esp_vfs_spiffs_conf_t *conf);
/*  получаем содержимое файла в виде блока памяти - при этом первый int - это размер буфера.
    с 5-го байта начинаются данные из файла (первые четыре байта буфера - размер последующего
    буфера с данными, без учёта первых 4-х байт, отданых под число размера буфера):
*/
int* spiffs_get_file_as_buf(char *file_name);
// сохраняем буфер в файл:
int spiffs_save_buf_to_file(char*file_name, void*buf,  int buf_size);

#endif /* SPIFFS_H_ */

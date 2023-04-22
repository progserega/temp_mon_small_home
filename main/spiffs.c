/* SPIFFS filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "spiffs.h"

static const char *TAG = "spiffs";

void spiffs_init(esp_vfs_spiffs_conf_t *conf, int max_files)
{
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
  ESP_LOGI(TAG, "Initializing SPIFFS struct");
  conf->base_path = "/spiffs";
  conf->partition_label = NULL;
  conf->max_files = max_files;
  conf->format_if_mount_failed = true;
}

int spiffs_mount(esp_vfs_spiffs_conf_t *conf)
{
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
  ESP_LOGI(TAG, "Initializing SPIFFS");

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register(conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return -1;
  }
  return 0;
}

int spiffs_info(esp_vfs_spiffs_conf_t *conf)
{
  size_t total = 0, used = 0;
  esp_err_t ret;
  ret = esp_spiffs_info(conf->partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    return -1;
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return 0;
  }
}

int* spiffs_get_file_as_buf(char *file_name)
{
  char path_buf[120];
  void *mem;
  FILE* fd;
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
  // Use POSIX and C standard library functions to work with files.
  // First create a file.
  sprintf(path_buf,"/spiffs/%s",file_name);
  ESP_LOGI(TAG, "Opening file for read: %s",path_buf);

  // Check if destination file exists before renaming
  struct stat st;
  if (stat(path_buf, &st) != 0) {
    // отсутствует файл:
    ESP_LOGW(TAG,"file not exist: %s",path_buf);
    return NULL;
  }
  fd = fopen(path_buf, "r");
  if (fd == NULL) {
    ESP_LOGE(TAG, "Failed to open file for read");
    return NULL;
  }
  // выделяем память + int32_t на хранение размера буфера - первым числом в буфере:
  mem = malloc(st.st_size+sizeof(int32_t));
  if(mem==NULL){
    ESP_LOGE(TAG, "Failed malloc %d bytes",(int)(st.st_size+sizeof(int)));
    return NULL;
  }
  // сохраняем размер буфера в его начало:
  *((int32_t*)mem)=st.st_size;
  if(fread(mem+sizeof(int32_t),st.st_size, sizeof(char),fd)==0){
    ESP_LOGE(TAG, "Failed read(%d) bytes from file %s",(int)st.st_size,path_buf);
    return NULL;
  }
  fclose(fd);
  return mem;
}

int spiffs_save_buf_to_file(char *file_name, void*buf, int buf_size)
{
  char path_buf[120];
  FILE* fd;
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
  // Use POSIX and C standard library functions to work with files.
  // First create a file.
  sprintf(path_buf,"/spiffs/%s",file_name);
  ESP_LOGI(TAG, "Opening file for rewrite: %s",path_buf);

  // запись с обнулением первоначальных данных:
  fd = fopen(path_buf, "w+");
  if (fd == NULL) {
    ESP_LOGE(TAG, "Failed to open file for write");
    return -1;
  }
  if(fwrite(buf,buf_size,sizeof(int8_t),fd)==-1){
    ESP_LOGE(TAG, "Failed write %d bytesto file: %s",buf_size,path_buf);
    return -1;
  }
  fclose(fd);
  return 0;
}

int spiffs_umount(esp_vfs_spiffs_conf_t *conf)
{
  ESP_LOGD(TAG,"%s(%d): start",__func__,__LINE__);
  // All done, unmount partition and disable SPIFFS
  esp_vfs_spiffs_unregister(conf->partition_label);
  ESP_LOGI(TAG, "SPIFFS unmounted");
  return 0;
}

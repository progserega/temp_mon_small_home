#include "http.h"
//-------------------------------------------------------------
static const char *TAG = "http";
//-------------------------------------------------------------
// index.html
const char http_header[] = {"HTTP/1.1 200 OK\r\nServer: nginx\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"};
const char index_htm[] = "<!DOCTYPE html><html lang=\"ru\"><head>	<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>ESP32</title></head><body><h1 style=\"text-align: center;\">Esp 32<br><br>HTTP Server</h1><p><span style=\"font-family: Times New Roman,Times,serif;\">ESP32 is a single 2.4 GHz Wi-Fi-and-Bluetooth combo chip designed with the TSMC ultra-low-power 40 nm technology.</span> </p></body></html>";
// index.json
const char json_header[] = {"HTTP/1.1 200 OK\r\nServer: nginx\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"};
const char index_json[] = "{\"name\":\"test\"}";
//-------------------------------------------------------------

// возвращает адрес нового буфера с полученной строкой, или NULL, в случае ошибки,
// в *buf_size записывает размер нового буфера
char* append_string(char* buf, int *buf_size, char *str)
{
  int buf_len=0;
  char *new_buf=NULL;
  int new_len=0;
  int buf_new_size=*buf_size;

  ESP_LOGD(TAG,"%s(%d): start: buf=%p, buf_size=%d",__func__,__LINE__,buf,*buf_size);
  // выделяем, если нужно память:
  if(*buf_size==0){
    buf_new_size=1024;
    new_buf=malloc(buf_new_size);
    if(new_buf==NULL){
      ESP_LOGE(TAG,"%s(%d): malloc()",__func__,__LINE__);
      *buf_size=0;
      return NULL;
    }
    new_len=strlen(str);
  }
  else{
    buf_len=strlen(buf);
    new_len=buf_len+strlen(str);
    // увеличиваем размер массива, чтобы поместилась добавляемая строка:
    while(buf_new_size-1<new_len){
      buf_new_size+=1024;
    }
    ESP_LOGD(TAG,"%s(%d): buf_new_size=%d",__func__,__LINE__,buf_new_size);
    if(buf_new_size>*buf_size){
      // расширяем буфер:
      new_buf=realloc(buf,buf_new_size);
      if(new_buf==NULL){
        ESP_LOGE(TAG,"%s(%d): realloc(%d)",__func__,__LINE__,buf_new_size);
        if(buf)free(buf);
        return NULL;
      }
    }
    else{
      // выделять новый блок не нужно - достаточно старого:
      new_buf=buf;
    }
  }
  // добавляем строку:
  strcpy(new_buf+buf_len,str);
  *(new_buf+new_len)=0;// завершающий символ нуля
  *buf_size=buf_new_size;
  return new_buf;
}
char *create_json(TEMPERATURE_data* td)
{
#define ADDSTR(buf, size, str) buf=append_string(buf,size,str);if(!buf){ESP_LOGE(TAG,"%s(%d): append_string()",__func__,__LINE__);return NULL;}
  char *buf=NULL;
  int buf_size=0;
  char tmp[256];
  bool list_empty;
  TEMPERATURE_device *dev;
  TEMPERATURE_stat_item *cur_stat_item;
  //buf=append_string(buf,&buf_size,"{");
  //if(!buf){ESP_LOGE(TAG,"%s(%d): append_string()",__func__,__LINE__);return NULL;}
  ADDSTR(buf,&buf_size,"{");
  ESP_LOGD(TAG,"%s(%d): xSemaphoreTake(temperature_data_sem)",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);
  for(int i=0;i<td->num_devices;i++)
  {
    dev = td->temp_devices+i;
    sprintf(tmp,"\"%s\":{",dev->device_addr);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"name\":\"%s\",",dev->device_name);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"addr\":\"%s\",",dev->device_addr);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"current_temperature\":%3.3f,",dev->temp);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_day_max_temp\":%3.3f,",(float)dev->stat_day_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_day_min_temp\":%3.3f,",(float)dev->stat_day_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_month_max_temp\":%3.3f,",(float)dev->stat_month_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_month_min_temp\":%3.3f,",(float)dev->stat_month_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_year_max_temp\":%3.3f,",(float)dev->stat_year_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_year_min_temp\":%3.3f,",(float)dev->stat_year_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"errors\":%d,",dev->errors);
    ADDSTR(buf,&buf_size,tmp);
    // статистика:
    ADDSTR(buf,&buf_size,"\"statistics\":{");

    // статитсика за сутки:
    ADDSTR(buf,&buf_size,"\"last_day_by_hours\":{");
    list_empty=true;
    for(int x=0;x<24;x++){
      cur_stat_item=dev->stat_day+x;
      if (cur_stat_item->min==ERROR_TEMPERATURE || cur_stat_item->max==ERROR_TEMPERATURE)continue;

      if(!list_empty){
        ADDSTR(buf,&buf_size,",");
      }

      sprintf(tmp,"\"%d\": {",x);
      ADDSTR(buf,&buf_size,tmp);

      sprintf(tmp,"\"min\": %f,",(float)cur_stat_item->min / 1000);
      ADDSTR(buf,&buf_size,tmp);
      sprintf(tmp,"\"max\": %f",(float)cur_stat_item->max / 1000);
      ADDSTR(buf,&buf_size,tmp);

      ADDSTR(buf,&buf_size,"}");
      list_empty=false;
    }
    // закрываем объект статистики "день":
    ADDSTR(buf,&buf_size,"},");

    // статитсика за месяц:
    ADDSTR(buf,&buf_size,"\"last_month_by_days\":{");
    list_empty=true;
    for(int x=0;x<31;x++){
      cur_stat_item=dev->stat_month+x;
      if (cur_stat_item->min==ERROR_TEMPERATURE || cur_stat_item->max==ERROR_TEMPERATURE)continue;

      if(!list_empty){
        ADDSTR(buf,&buf_size,",");
      }

      sprintf(tmp,"\"%d\": {",x+1);
      ADDSTR(buf,&buf_size,tmp);

      sprintf(tmp,"\"min\": %f,",(float)cur_stat_item->min / 1000);
      ADDSTR(buf,&buf_size,tmp);
      sprintf(tmp,"\"max\": %f",(float)cur_stat_item->max / 1000);
      ADDSTR(buf,&buf_size,tmp);

      ADDSTR(buf,&buf_size,"}");
      list_empty=false;
    }
    // закрываем объект статистики "месяц":
    ADDSTR(buf,&buf_size,"},");

    // статитсика за год:
    ADDSTR(buf,&buf_size,"\"last_year_by_months\":{");
    list_empty=true;
    for(int x=0;x<12;x++){
      cur_stat_item=dev->stat_year+x;
      if (cur_stat_item->min==ERROR_TEMPERATURE || cur_stat_item->max==ERROR_TEMPERATURE)continue;

      if(!list_empty){
        ADDSTR(buf,&buf_size,",");
      }

      sprintf(tmp,"\"%d\": {",x+1);
      ADDSTR(buf,&buf_size,tmp);

      sprintf(tmp,"\"min\": %f,",(float)cur_stat_item->min / 1000);
      ADDSTR(buf,&buf_size,tmp);
      sprintf(tmp,"\"max\": %f",(float)cur_stat_item->max / 1000);
      ADDSTR(buf,&buf_size,tmp);

      ADDSTR(buf,&buf_size,"}");
      list_empty=false;
    }
    // закрываем объект статистики "год":
    ADDSTR(buf,&buf_size,"}");

    // закрываем объект статистики:
    // последний объект без запятой после себя:
    ADDSTR(buf,&buf_size,"}");

    // закрываем объект датчика:
    ADDSTR(buf,&buf_size,"}");
    // последний объект без запятой после себя:
    if(i<td->num_devices-1){
      ADDSTR(buf,&buf_size,",");
    }
  }
  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive(temperature_data_sem)",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
  ADDSTR(buf,&buf_size,"}");
  return buf;
}

void http_task(void *pvParameters)
{
  int sockfd, accept_sock, ret;
  socklen_t sockaddrsize;
  uint8_t *buf;
  char *json_data;
  int buflen = 1024;
  struct sockaddr_in servaddr, cliaddr;
  ESP_LOGI(TAG,"%s(%d): Create socket...",__func__,__LINE__);
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0 ) {
    ESP_LOGE(TAG, "socket not created");
    vTaskDelete(NULL);
  }
  ESP_LOGI(TAG,"%s(%d): Socket created",__func__,__LINE__);
  memset(&servaddr, 0, sizeof(servaddr));
  //Заполнение информации о сервере
  servaddr.sin_family    = AF_INET; // IPv4
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(CONFIG_SERVER_PORT);
  //Свяжем сокет с адресом сервера
  if (bind(sockfd, (const struct sockaddr *)&servaddr,  sizeof(struct sockaddr_in)) < 0 )
  {
    ESP_LOGE(TAG,"%s(%d): socket not binded",__func__,__LINE__);
    vTaskDelete(NULL);
  }
  ESP_LOGI(TAG,"%s(%d): socket was binded",__func__,__LINE__);
  listen(sockfd, 5);
  while(1)
  {
    memset(&cliaddr, 0, sizeof(cliaddr));
    accept_sock = accept(sockfd, (struct sockaddr *)&cliaddr, (socklen_t *)&sockaddrsize);
    ESP_LOGD(TAG,"%s(%d): socket: %d",__func__,__LINE__, accept_sock);
    if(accept_sock >= 0)
    {
      buf = heap_caps_malloc(1024, MALLOC_CAP_8BIT);
      ret = recvfrom(accept_sock, buf, buflen, 0, (struct sockaddr *)&cliaddr, &sockaddrsize);
      if(ret > 0)
      {
        if ((ret >=5 ) && (strncmp((char*)buf, "GET /", 5) == 0))
        {
          if ((strncmp((char const *)buf,"GET / ",6)==0)||(strncmp((char const *)buf,"GET /index.html",15)==0))
          {
            strcpy((char*)buf,http_header);
            memcpy((void*)(buf + strlen(http_header)),(void*)index_htm,sizeof(index_htm));
            write(accept_sock, (const unsigned char*)buf, strlen(http_header) + sizeof(index_htm));
          }
          else if (strncmp((char const *)buf,"GET /index.json",15)==0)
          {
            /*strcpy((char*)buf,json_header);
            memcpy((void*)(buf + strlen(json_header)),(void*)index_json,sizeof(index_json));
            write(accept_sock, (const unsigned char*)buf, strlen(json_header) + sizeof(index_json)-1);
            */
           // strcpy((char*)buf,json_header);
            ESP_LOGI(TAG,"%s(%d): create json_data",__func__,__LINE__);
            write(accept_sock, (const unsigned char*)json_header, strlen(json_header));
            json_data=create_json((TEMPERATURE_data*)pvParameters);
            ESP_LOGD(TAG,"%s(%d): json_data=%s",__func__,__LINE__,json_data);
            if(!json_data){ESP_LOGE(TAG,"%s(%d): create_json(TEMPERATURE_data*td)",__func__,__LINE__);}
            else{
              write(accept_sock, (const unsigned char*)json_data,strlen(json_data));
              free(json_data);
            }
          }
        }
      }
      close(accept_sock);
      free(buf);
    }
  }
  close(sockfd);
  vTaskDelete(NULL);
}
//-------------------------------------------------------------

#include "http.h"
#include "sys_time.h"
//-------------------------------------------------------------
static const char *TAG = "http";
//-------------------------------------------------------------
// index.html
const char http_header[] = {"HTTP/1.1 200 OK\r\nServer: nginx\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"};
// index.json
const char json_header[] = {"HTTP/1.1 200 OK\r\nServer: nginx\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"};
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

void http_task(void *pvParameters)
{
  int sockfd, accept_sock, ret;
  socklen_t sockaddrsize;
  uint8_t *buf;
  char *data;
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
            ESP_LOGI(TAG,"%s(%d): create html_data",__func__,__LINE__);
            write(accept_sock, (const unsigned char*)http_header, strlen(http_header));
            data=create_index((TEMPERATURE_data*)pvParameters);
            ESP_LOGD(TAG,"%s(%d): index_data=%s",__func__,__LINE__,data);
            if(!data){ESP_LOGE(TAG,"%s(%d): create_index(TEMPERATURE_data*td)",__func__,__LINE__);}
            else{
              write(accept_sock, (const unsigned char*)data,strlen(data));
              free(data);
            }
          }
          else if (strncmp((char const *)buf,"GET /index.json",15)==0)
          {
            ESP_LOGI(TAG,"%s(%d): create json_data",__func__,__LINE__);
            write(accept_sock, (const unsigned char*)json_header, strlen(json_header));
            data=create_json((TEMPERATURE_data*)pvParameters);
            ESP_LOGD(TAG,"%s(%d): json_data=%s",__func__,__LINE__,data);
            if(!data){ESP_LOGE(TAG,"%s(%d): create_json(TEMPERATURE_data*td)",__func__,__LINE__);}
            else{
              write(accept_sock, (const unsigned char*)data,strlen(data));
              free(data);
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


// формируем страничку index.json со статистикой для машины:
char *create_json(TEMPERATURE_data* td)
{
#define ADDSTR(buf, size, str) buf=append_string(buf,size,str);if(!buf){ESP_LOGE(TAG,"%s(%d): append_string()",__func__,__LINE__);return NULL;}
  char *buf=NULL;
  int buf_size=0;
  int x;
  char tmp[256];
  bool list_empty;
  TEMPERATURE_device *dev;
  TEMPERATURE_stat_item *cur_stat_item;
  //buf=append_string(buf,&buf_size,"{");
  //if(!buf){ESP_LOGE(TAG,"%s(%d): append_string()",__func__,__LINE__);return NULL;}
  ADDSTR(buf,&buf_size,"{");
  ADDSTR(buf,&buf_size,"\"temp_sensors\":{");
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
    sprintf(tmp,"\"current_temperature\":%3.2f,",dev->temp);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_day_max_temp\":%3.2f,",(float)dev->stat_day_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_day_min_temp\":%3.2f,",(float)dev->stat_day_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_month_max_temp\":%3.2f,",(float)dev->stat_month_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_month_min_temp\":%3.2f,",(float)dev->stat_month_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_year_max_temp\":%3.2f,",(float)dev->stat_year_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"stat_year_min_temp\":%3.2f,",(float)dev->stat_year_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"\"errors\":%d,",dev->errors);
    ADDSTR(buf,&buf_size,tmp);
    // статистика:
    ADDSTR(buf,&buf_size,"\"statistics\":{");

    // статитсика за сутки:
    ADDSTR(buf,&buf_size,"\"last_day_by_hours\":{");
    list_empty=true;
    for(x=0;x<24;x++){
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
    for(x=0;x<31;x++){
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
    for(x=0;x<12;x++){
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
  // закрываем объект данных по температурным датчикам:
  ADDSTR(buf,&buf_size,"},");
  ADDSTR(buf,&buf_size,"\"system\":{");
  // закрываем объект данных по системе
  sprintf(tmp,"\"uptime_seconds\": %llu",esp_timer_get_time()/1000000);
  ADDSTR(buf,&buf_size,tmp);
  ADDSTR(buf,&buf_size,"}");
  // закрываем json
  ADDSTR(buf,&buf_size,"}");
  return buf;
}


// формируем страничку index.html со статистикой для человека:
char* create_index(TEMPERATURE_data *td)
{
#define ADDSTR(buf, size, str) buf=append_string(buf,size,str);if(!buf){ESP_LOGE(TAG,"%s(%d): append_string()",__func__,__LINE__);return NULL;}
  char *buf=NULL;
  int buf_size=0;
  char tmp[512];
  char *tmp_pointer;
  int x;
  TEMPERATURE_device *dev;
  TEMPERATURE_stat_item *cur_stat_item;
  ADDSTR(buf,&buf_size,"<!DOCTYPE html> <html lang=\"ru\">\
  <head>\
  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\
  <title>Датчики в моём доме</title>  \
  \
<style> \
td { \
background-color: lightblue; \
}\
th { \
background-color: lightgreen; \
}\
.red {\
background-color: #ff8080;\
}\
.orange {\
background-color: #f4d058;\
}\
.current {\
background-color: green;\
color: white;\
}\
</style>\
</head>\
<body>");
  ESP_LOGD(TAG,"%s(%d): xSemaphoreTake(temperature_data_sem)",__func__,__LINE__);xSemaphoreTake(temperature_data_sem,portMAX_DELAY);

  /// uptime выводим на экран:
  ADDSTR(buf,&buf_size,"Время работы системы с этапа загрузки составляет: ");
  tmp_pointer=systime_uptime();
  if(tmp_pointer==NULL){
    ESP_LOGE(TAG,"systime_uptime()");
    ADDSTR(buf,&buf_size,"неизвестно");
  }
  else{
    ADDSTR(buf,&buf_size,tmp_pointer);
    free(tmp_pointer);
  }
  ADDSTR(buf,&buf_size,"<br>");

  ADDSTR(buf,&buf_size,"<h1 style=\"text-align: center;\">Температуры датчиков:</h1>");
  sprintf(tmp,"<table border=\"1\"><thead><tr><th>Наименование датчика</th>\
  <th>Текущая температура</th>\
  <th>Максимальная за день</th>\
  <th>Минимальная за день</th>\
  <th>Максимальная за месяц</th>\
  <th>Минимальная за месяц</th>\
  <th>Максимальная за год</th>\
  <th>Минимальная за год</th>\
  </tr></thead><tbody>");
  ADDSTR(buf,&buf_size,tmp);
  for(int i=0;i<td->num_devices;i++) 
  {
    dev = td->temp_devices+i;
    sprintf(tmp,"<tr>");
    ADDSTR(buf,&buf_size,tmp);

    // имя
    if (strlen(dev->device_name)==0){
      sprintf(tmp,"<td>%s</td>",dev->device_addr);
    }
    else{
      sprintf(tmp,"<td>%s</td>",dev->device_name);
    }
    ADDSTR(buf,&buf_size,tmp);

    // текущая температура:
    if(dev->temp==ERROR_TEMPERATURE){
      sprintf(tmp,"<td class=\"red\">-</td>");
    }else{
      sprintf(tmp,"<td class=\"current\">%3.2f</td>",dev->temp);
    }
    ADDSTR(buf,&buf_size,tmp);

    // максимальная за сутки:
    if(dev->stat_day_max_temp==ERROR_TEMPERATURE){
      sprintf(tmp,"<td class=\"red\">-</td>");
    }else{
      sprintf(tmp,"<td class=\"orange\">%3.2f</td>",(float)dev->stat_day_max_temp/1000);
    }
    ADDSTR(buf,&buf_size,tmp);
    // минимальная за сутки:
    if(dev->stat_day_min_temp==ERROR_TEMPERATURE){
      sprintf(tmp,"<td class=\"red\">-</td>");
    }else{
      sprintf(tmp,"<td>%3.2f</td>",(float)dev->stat_day_min_temp/1000);
    }
    ADDSTR(buf,&buf_size,tmp);

    // максимальная за месяц:
    if(dev->stat_month_max_temp==ERROR_TEMPERATURE){
      sprintf(tmp,"<td class=\"red\">-</td>");
    }else{
      sprintf(tmp,"<td class=\"orange\">%3.2f</td>",(float)dev->stat_month_max_temp/1000);
    }
    ADDSTR(buf,&buf_size,tmp);
    // минимальная за месяц:
    if(dev->stat_month_min_temp==ERROR_TEMPERATURE){
      sprintf(tmp,"<td class=\"red\">-</td>");
    }else{
      sprintf(tmp,"<td>%3.2f</td>",(float)dev->stat_month_min_temp/1000);
    }
    ADDSTR(buf,&buf_size,tmp);

    // максимальная за год:
    if(dev->stat_year_max_temp==ERROR_TEMPERATURE){
      sprintf(tmp,"<td class=\"red\">-</td>");
    }else{
      sprintf(tmp,"<td class=\"orange\">%3.2f</td>",(float)dev->stat_year_max_temp/1000);
    }
    ADDSTR(buf,&buf_size,tmp);
    // минимальная за год:
    if(dev->stat_year_min_temp==ERROR_TEMPERATURE){
      sprintf(tmp,"<td class=\"red\">-</td>");
    }else{
      sprintf(tmp,"<td>%3.2f</td>",(float)dev->stat_year_min_temp/1000);
    }
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"</tr>");
    ADDSTR(buf,&buf_size,tmp);
  }
  sprintf(tmp,"</tbody></table>");
  ADDSTR(buf,&buf_size,tmp);

  ADDSTR(buf,&buf_size,"<h1 style=\"text-align: center;\">Статистика температурных датчиков</h1>");
  for(int i=0;i<td->num_devices;i++) 
  {
    dev = td->temp_devices+i;
    if (strlen(dev->device_name)==0){
      sprintf(tmp,"<h3 style=\"text-align: center;\"> Имя датчика: %s</h3>",dev->device_addr);
    }
    else{
      sprintf(tmp,"<h3 style=\"text-align: center;\"> Имя датчика: %s</h3>",dev->device_name);
    }
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<table border=\"1\"><thead><tr><th>Наименование показания</th><th>Показания в градусах</th></tr></thead><tbody>");
    ADDSTR(buf,&buf_size,tmp);

    sprintf(tmp,"<tr><td>Текущая температура</td><td class=\"current\">%3.2f C</td></tr>,",dev->temp);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><td class=\"orange\">Максимальная температура за последние сутки</td><td class=\"orange\">%3.2f C</td></tr>,",(float)dev->stat_day_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><td>Минимальная температура за последние сутки</td><td>%3.2f C</td></tr>,",(float)dev->stat_day_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><td class=\"orange\">Максимальная температура за последний месяц</td><td class=\"orange\">%3.2f C</td></tr>,",(float)dev->stat_month_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><td>Минимальная температура за последний месяц</td><td>%3.2f C</td></tr>,",(float)dev->stat_month_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><td class=\"orange\">Максимальная температура за последний год</td><td class=\"orange\">%3.2f C</td></tr>,",(float)dev->stat_year_max_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><td>Минимальная температура за последний год</td><td>%3.2f C</td></tr>,",(float)dev->stat_year_min_temp/1000);
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"</tbody></table>");
    ADDSTR(buf,&buf_size,tmp);

    // статитсика за сутки:
    sprintf(tmp,"<h4>Статистика за сутки:</h4>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<table border=\"1\"><thead><tr><th></th><th colspan=\"24\">Часы суток</th></tr>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><th>Тип значения</th>");
    ADDSTR(buf,&buf_size,tmp);
    for(x=0;x<24;x++){
      sprintf(tmp,"<th>%d</th>",x);
      ADDSTR(buf,&buf_size,tmp);
    }
    sprintf(tmp,"</tr></thead><tbody>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<td class=\"orange\">Максимальные зафиксированные значения (C)</td>");
    ADDSTR(buf,&buf_size,tmp);
    for(int x=0;x<24;x++){
      cur_stat_item=dev->stat_day+x;
      if (cur_stat_item->max==ERROR_TEMPERATURE){
        sprintf(tmp,"<td class=\"red\">-</td>");
        ADDSTR(buf,&buf_size,tmp);
      }
      else{
        sprintf(tmp,"<td class=\"orange\">%3.2f</td>",(float)cur_stat_item->max/1000);
        ADDSTR(buf,&buf_size,tmp);
      }
    }
    sprintf(tmp,"</tr><td>Минимальные зафиксированные значения (C)</td>");
    ADDSTR(buf,&buf_size,tmp);
    for(int x=0;x<24;x++){
      cur_stat_item=dev->stat_day+x;
      if (cur_stat_item->min==ERROR_TEMPERATURE){
        sprintf(tmp,"<td class=\"red\">-</td>");
        ADDSTR(buf,&buf_size,tmp);
      }
      else{
        sprintf(tmp,"<td>%3.2f</td>",(float)cur_stat_item->min/1000);
        ADDSTR(buf,&buf_size,tmp);
      }
    }
    sprintf(tmp,"</tr></tbody></table>");
    ADDSTR(buf,&buf_size,tmp);


    // статитсика за месяц:
    sprintf(tmp,"<h4>Статистика за последний месяц:</h4>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<table border=\"1\"><thead><tr><th></th><th colspan=\"31\">Дни месяца</th></tr>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><th>Тип значения</th>");
    ADDSTR(buf,&buf_size,tmp);
    for(x=1;x<=31;x++){
      sprintf(tmp,"<th>%d</th>",x);
      ADDSTR(buf,&buf_size,tmp);
    }
    sprintf(tmp,"</tr></thead><tbody>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<td class=\"orange\">Максимальные зафиксированные значения (C)</td>");
    ADDSTR(buf,&buf_size,tmp);
    for(int x=0;x<31;x++){
      cur_stat_item=dev->stat_month+x;
      if (cur_stat_item->max==ERROR_TEMPERATURE){
        sprintf(tmp,"<td class=\"red\">-</td>");
        ADDSTR(buf,&buf_size,tmp);
      }
      else{
        sprintf(tmp,"<td class=\"orange\">%3.2f</td>",(float)cur_stat_item->max/1000);
        ADDSTR(buf,&buf_size,tmp);
      }
    }
    sprintf(tmp,"</tr><td>Минимальные зафиксированные значения (C)</td>");
    ADDSTR(buf,&buf_size,tmp);
    for(int x=0;x<31;x++){
      cur_stat_item=dev->stat_month+x;
      if (cur_stat_item->min==ERROR_TEMPERATURE){
        sprintf(tmp,"<td class=\"red\">-</td>");
        ADDSTR(buf,&buf_size,tmp);
      }
      else{
        sprintf(tmp,"<td>%3.2f</td>",(float)cur_stat_item->min/1000);
        ADDSTR(buf,&buf_size,tmp);
      }
    }
    sprintf(tmp,"</tr></tbody></table>");
    ADDSTR(buf,&buf_size,tmp);

    // статитсика за год:
    sprintf(tmp,"<h4>Статистика за последний год:</h4>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<table border=\"1\"><thead><tr><th></th><th colspan=\"31\">Месяцы года</th></tr>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<tr><th>Тип значения</th>");
    ADDSTR(buf,&buf_size,tmp);
    for(x=1;x<=12;x++){
      sprintf(tmp,"<th>%d</th>",x);
      ADDSTR(buf,&buf_size,tmp);
    }
    sprintf(tmp,"</tr></thead><tbody>");
    ADDSTR(buf,&buf_size,tmp);
    sprintf(tmp,"<td class=\"orange\">Максимальные зафиксированные значения (C)</td>");
    ADDSTR(buf,&buf_size,tmp);
    for(int x=0;x<12;x++){
      cur_stat_item=dev->stat_year+x;
      if (cur_stat_item->max==ERROR_TEMPERATURE){
        sprintf(tmp,"<td class=\"red\">-</td>");
        ADDSTR(buf,&buf_size,tmp);
      }
      else{
        sprintf(tmp,"<td class=\"orange\">%3.2f</td>",(float)cur_stat_item->max/1000);
        ADDSTR(buf,&buf_size,tmp);
      }
    }
    sprintf(tmp,"</tr><td>Минимальные зафиксированные значения (C)</td>");
    ADDSTR(buf,&buf_size,tmp);
    for(int x=0;x<12;x++){
      cur_stat_item=dev->stat_year+x;
      if (cur_stat_item->min==ERROR_TEMPERATURE){
        sprintf(tmp,"<td class=\"red\">-</td>");
        ADDSTR(buf,&buf_size,tmp);
      }
      else{
        sprintf(tmp,"<td>%3.2f</td>",(float)cur_stat_item->min/1000);
        ADDSTR(buf,&buf_size,tmp);
      }
    }
    sprintf(tmp,"</tr></tbody></table>");
    ADDSTR(buf,&buf_size,tmp);

  }
  ESP_LOGD(TAG,"%s(%d): xSemaphoreGive(temperature_data_sem)",__func__,__LINE__);xSemaphoreGive(temperature_data_sem);
  ADDSTR(buf,&buf_size,"</body></html>");
  return buf;
}

//-------------------------------------------------------------

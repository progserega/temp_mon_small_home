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

  ESP_LOGD(TAG,"(%s:%d): %s(): start: buf=%p, buf_size=%d",__FILE__,__LINE__,__func__,buf,*buf_size);
  // выделяем, если нужно память:
  if(*buf_size==0){
    buf_new_size=1024;
    new_buf=malloc(buf_new_size);
    if(new_buf==NULL){
      ESP_LOGE(TAG,"(%s:%d): %s(): malloc()",__FILE__,__LINE__,__func__);
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
    ESP_LOGD(TAG,"(%s:%d): %s(): buf_new_size=%d",__FILE__,__LINE__,__func__,buf_new_size);
    if(buf_new_size>*buf_size){
      // расширяем буфер:
      new_buf=realloc(buf,buf_new_size);
      if(new_buf==NULL){
        ESP_LOGE(TAG,"(%s:%d): %s(): realloc(%d)",__FILE__,__LINE__,__func__,buf_new_size);
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
  char *buf=NULL;
  int buf_size=0;
  buf=append_string(buf,&buf_size,"{");
  ESP_LOGW(TAG,"(%s:%d): %s(): buf=%s",__FILE__,__LINE__,__func__,buf);
  if(!buf){ESP_LOGE(TAG,"(%s:%d): %s(): append_string()",__FILE__,__LINE__,__func__);return NULL;}
  buf=append_string(buf,&buf_size,"\"name\":\"test\",");
  ESP_LOGW(TAG,"(%s:%d): %s(): buf=%s",__FILE__,__LINE__,__func__,buf);
  if(!buf){ESP_LOGE(TAG,"(%s:%d): %s(): append_string()",__FILE__,__LINE__,__func__);return NULL;}
  buf=append_string(buf,&buf_size,"\"name2\":\"test2\"");
  if(!buf){ESP_LOGE(TAG,"(%s:%d): %s(): append_string()",__FILE__,__LINE__,__func__);return NULL;}
  buf=append_string(buf,&buf_size,"}");
  if(!buf){ESP_LOGE(TAG,"(%s:%d): %s(): append_string()",__FILE__,__LINE__,__func__);return NULL;}
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
  ESP_LOGI(TAG, "Create socket...");
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0 ) {
    ESP_LOGE(TAG, "socket not created");
    vTaskDelete(NULL);
  }
  ESP_LOGI(TAG, "Socket created");
  memset(&servaddr, 0, sizeof(servaddr));
  //Заполнение информации о сервере
  servaddr.sin_family    = AF_INET; // IPv4
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(CONFIG_SERVER_PORT);
  //Свяжем сокет с адресом сервера
  if (bind(sockfd, (const struct sockaddr *)&servaddr,  sizeof(struct sockaddr_in)) < 0 )
  {
    ESP_LOGE(TAG, "socket not binded");
    vTaskDelete(NULL);
  }
  ESP_LOGI(TAG, "socket was binded");
  listen(sockfd, 5);
  while(1)
  {
    memset(&cliaddr, 0, sizeof(cliaddr));
    accept_sock = accept(sockfd, (struct sockaddr *)&cliaddr, (socklen_t *)&sockaddrsize);
    printf("socket: %d\n", accept_sock);
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
            ESP_LOGI(TAG,"(%s:%d): %s(): create json_data",__FILE__,__LINE__,__func__);
            ESP_LOGD(TAG,"(%s:%d): %s(): create json_data",__FILE__,__LINE__,__func__);
            write(accept_sock, (const unsigned char*)json_header, strlen(json_header));
            json_data=create_json((TEMPERATURE_data*)pvParameters);
            ESP_LOGD(TAG,"(%s:%d): %s(): json_data=%s",__FILE__,__LINE__,__func__,json_data);
            if(!json_data){ESP_LOGE(TAG,"(%s:%d): %s(): create_json(TEMPERATURE_data*td)",__FILE__,__LINE__,__func__);}
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

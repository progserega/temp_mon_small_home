#include "sys_time.h"

static const char *TAG = "sys_time";

void esp_initialize_sntp(void)
{
    ESP_LOGI(TAG, "%s(%d): Initializing SNTP",__func__,__LINE__);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "0.ru.pool.ntp.org");
    sntp_setservername(1, "1.ru.pool.ntp.org");
    sntp_setservername(2, "pool.ntp.org");
    sntp_init();
}

bool esp_wait_sntp_sync(int wait_seconds)
{
    char strftime_buf[64];
    esp_initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    bool ret = true;

    while (timeinfo.tm_year < (2019 - 1900)) {
        ESP_LOGD(TAG, "%s(%d): Waiting for system time to be set... (%d)",__func__,__LINE__, ++retry);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
        if(wait_seconds!=0){
          if(retry>=wait_seconds){
            // закончились попытки:
            ret=false;
            ESP_LOGW(TAG, "%s(%d): End try update time...",__func__,__LINE__);
            break;
          }
        }

    }

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "%s(%d): The current date/time before set time zone: %s",__func__,__LINE__,strftime_buf);

    ESP_LOGD(TAG, "%s(%d): set time zone",__func__,__LINE__);
    //setenv("TZ", "MSK-07", 1);
    setenv("TZ", "UTC-10", 1);
    tzset();

    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "%s(%d): The current date/time after set time zone: %s",__func__,__LINE__,strftime_buf);
    return ret;
}

// Уровень приложения может напрямую вызывать esp_wait_sntp_sync ().

char* systime_uptime(void)
{
  int64_t uptime_microsec,uptime;
  int sec,min,hour,day;
  char *buf;
  uptime_microsec = esp_timer_get_time();
  uptime = uptime_microsec/1000000;
  sec=(int)((uptime)%60);
  min=(int)(((uptime)%3600)/60);
  hour=(int)(((uptime)%(3600*24))/3600);
  day=(int)((uptime)/(3600*24));
  buf=malloc(255);
  if(buf==NULL){
    ESP_LOGE(TAG,"malloc(255)");
    return NULL;
  }
  sprintf(buf,"%d дней %d:%d:%d часов/мин/сек",day,hour,min,sec);
  return buf;
}

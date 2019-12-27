#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"
#include "stdio.h"

static esp_err_t event_handler(void *ctx, system_event_t *event)
{

    switch(event->event_id) 
    {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGW("WiFI", "WiFi got IP from ESP");
            MQTT::getInstance()->wifiConnected();
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGW("WiFI", "WiFi Disconnect from ESP");
            MQTT::getInstance()->wifiDisconnected();
            //esp_wifi_connect();
            break;

        default:
            break;
    }
    return ESP_OK;
}

WiFi::WiFi()
{
    ESP_LOGI("WiFi", "init");
    alt = true;
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
}


void WiFi::connect_alt_wifi(void)
{

    wifi_config_t wifi_config = { .sta = 
                                    {
                                        CONFIG_WIFIALT_SSID,
                                        CONFIG_WIFIALT_PASSWORD,
                                        WIFI_FAST_SCAN,
                                        false,
                                        {'0','0','0','0','0','0'},
                                        0,
                                        0,
                                        WIFI_CONNECT_AP_BY_SIGNAL,
                                        {0, WIFI_AUTH_OPEN}
                                    }
                                };

    ESP_LOGI("WIFI", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void WiFi::connect_wifi(void)
{

    wifi_config_t wifi_config = { .sta = 
                                    {
                                        CONFIG_WIFI_SSID,
                                        CONFIG_WIFI_PASSWORD,
                                        WIFI_FAST_SCAN,
                                        false,
                                        {'0','0','0','0','0','0'},
                                        0,
                                        0,
                                        WIFI_CONNECT_AP_BY_SIGNAL,
                                        {0, WIFI_AUTH_OPEN}
                                    }
                                };

    ESP_LOGI("WIFI", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void WiFi::disconnect(void)
{
    ESP_ERROR_CHECK( esp_wifi_stop() );
}

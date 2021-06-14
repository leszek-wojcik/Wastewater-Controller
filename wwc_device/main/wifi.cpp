#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "MethodRequest.h"
#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"
#include "stdio.h"

#include "driver/gpio.h"

#define GPIO_INPUT_IO_22    22
#define GPIO_INPUT_PIN_SEL  1ULL<<(GPIO_INPUT_IO_22) 
#define ESP_INTR_FLAG_DEFAULT 0


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

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWokenByPost;
    if ( WiFi::interruptMr != NULL && WiFi::instance != NULL )
    {
        xQueueSendFromISR( WiFi::instance->mrQueue,
                           &(WiFi::interruptMr),
                           &xHigherPriorityTaskWokenByPost);
    }
}

WiFi* WiFi::instance = NULL;
MRequest * WiFi::interruptMr = NULL;

WiFi::WiFi():ActiveObject("WiFi",4096,7)
{
    ESP_LOGI("WiFi", "init");
    instance = this;
    alt = false;
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    gpio_config_t io_conf;
    io_conf.intr_type = (gpio_int_type_t) GPIO_PIN_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = (gpio_pullup_t) 1;
    io_conf.pull_down_en = (gpio_pulldown_t) 0;
    gpio_config(&io_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add( (gpio_num_t)GPIO_INPUT_IO_22, gpio_isr_handler, NULL);

    const std::function<void()> &f = [=](){ this->pushButton();};
    interruptMr = new MRequest( this, f);
    interruptMr->persistent = true;
}

void WiFi::pushButton()
{
    printf ("pushButton\n");
    gpio_isr_handler_remove( (gpio_num_t) GPIO_INPUT_IO_22 );
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

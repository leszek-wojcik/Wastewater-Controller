#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "MethodRequest.h"
#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"
#include "stdio.h"

#include "driver/gpio.h"

#define GPIO_INPUT_IO_22 22
#define GPIO_INPUT_PIN_SEL 1ULL << (GPIO_INPUT_IO_22)
#define ESP_INTR_FLAG_DEFAULT 0

#ifndef PIN2STR
#define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif

static void wifiEventHandler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGW("WiFI", "Attempt to connect");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW("WiFI", "WiFi Disconnect from ESP");
        MQTT::getInstance()->wifiDisconnected();
        //esp_wifi_connect();
        break;
    default:
        break;
    }
}

static void gotIPEventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI("WiFi", "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    MQTT::getInstance()->wifiConnected();

}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWokenByPost;
    if (WiFi::interruptMr != NULL && WiFi::instance != NULL)
    {
        xQueueSendFromISR(WiFi::instance->mrQueue,
                          &(WiFi::interruptMr),
                          &xHigherPriorityTaskWokenByPost);
    }
}

WiFi *WiFi::instance = NULL;
MRequest *WiFi::interruptMr = NULL;

WiFi::WiFi() : ActiveObject("WiFi", 4096, 7)
{
    ESP_LOGI("WiFi", "init");

    instance = this;
    alt = false;
    wps = 0;

    initWiFiStorage();
    initWPSButton();
    initWiFiDriver();
}

void WiFi::initWiFiWPSDriver()
{
//    ESP_ERROR_CHECK(esp_netif_init());
//    ESP_ERROR_CHECK(esp_event_loop_create_default());
//    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
//    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &gotIPEventHandler, NULL));
//    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//    ESP_ERROR_CHECK(esp_wifi_start());
//    ESP_LOGI("WiFi", "start wps...");
//    ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
//    ESP_ERROR_CHECK(esp_wifi_wps_start(0));
}

void WiFi::initWiFiDriver()
{
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &gotIPEventHandler, NULL));
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}

void WiFi::initWiFiStorage()
{
    esp_err_t err;
    err = nvs_open("wifi_storage", NVS_READWRITE, &dbHandle);
    if (err != ESP_OK) {
        ESP_LOGW("WiFi","Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        ESP_LOGI("WiFi","Done");

        // Read
        ESP_LOGI("WiFi","Reading wps from NVS wifi_wps ... ");
        err = nvs_get_i8(dbHandle, "wifi_wps", &wps);
        switch (err) {
            case ESP_OK:
                ESP_LOGI("WiFi", "Done");
                ESP_LOGI("WiFi", "wifi_wps = %d", wps);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI("WiFi","Initializing wifi_wps \n");
                wps = 0;
                ESP_LOGI("WiFi", "wifi_wps = %d", wps);
                break;
            default :
                printf("Error (%s) reading!", esp_err_to_name(err));
        }
    }
}

void WiFi::initWPSButton()
{
    gpio_config_t io_conf;
    io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = (gpio_pullup_t)1;
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add((gpio_num_t)GPIO_INPUT_IO_22, gpio_isr_handler, NULL);

    const std::function<void()> &f = [=]()
    { this->pushButton(); };
    interruptMr = new MRequest(this, f);
    interruptMr->persistent = true;
}

void WiFi::pushButton()
{
    ESP_LOGI("WiFi","WPS button pressed\n");
    esp_err_t err;
    gpio_isr_handler_remove((gpio_num_t)GPIO_INPUT_IO_22);
    wps = 1;
    
    err = nvs_set_i8(dbHandle, "wifi_wps", wps);
    
    ESP_LOGI("WiFi","Committing updates in NVS ... ");
    err = nvs_commit(dbHandle);
    ESP_LOGI("WiFi", "... %d", err );
    nvs_close(dbHandle);
    esp_restart();
}

void WiFi::connectWPS(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "start wps...");

    // ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
    // ESP_ERROR_CHECK(esp_wifi_wps_start(0));
}

void WiFi::connect_alt_wifi(void)
{

    wifi_config_t wifi_config = {.sta =
                                     {
                                         CONFIG_WIFIALT_SSID,
                                         CONFIG_WIFIALT_PASSWORD,
                                         WIFI_FAST_SCAN,
                                         false,
                                         {'0', '0', '0', '0', '0', '0'},
                                         0,
                                         0,
                                         WIFI_CONNECT_AP_BY_SIGNAL,
                                         {0, WIFI_AUTH_OPEN},
                                         {false, false},
                                         0, 0, 0
                                    }
                                };

    ESP_LOGI("WIFI", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFi::connect_wifi(void)
{

    wifi_config_t wifi_config = {.sta =
                                    {
                                        CONFIG_WIFI_SSID,
                                        CONFIG_WIFI_PASSWORD,
                                        WIFI_FAST_SCAN,
                                        false,
                                        {'0', '0', '0', '0', '0', '0'},
                                        0,
                                        0,
                                        WIFI_CONNECT_AP_BY_SIGNAL,
                                        {0, WIFI_AUTH_OPEN},
                                        {false, false},
                                        0, 0, 0
                                    }
                                };

    ESP_LOGI("WIFI", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFi::disconnect(void)
{
    ESP_ERROR_CHECK(esp_wifi_stop());
}

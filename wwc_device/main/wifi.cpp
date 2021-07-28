#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_handle.hpp"

#include "ActiveObject.h"
#include "MethodRequest.h"
#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"
#include "LEDService.h"
#include "stdio.h"


#include "driver/gpio.h"

#define GPIO_INPUT_IO_22 22
#define GPIO_INPUT_PIN_SEL 1ULL << (GPIO_INPUT_IO_22)
#define ESP_INTR_FLAG_DEFAULT 0

static void gotIPEventHandler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI("WiFi", "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    MQTT::getInstance()->wifiConnected();
}

static void wifiEventHandler (void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id)
    {
    
    case WIFI_EVENT_STA_START:
        ESP_LOGI("WiFI", "WIFI_EVENT_STA_START");
        esp_wifi_connect();
        break;
    
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW("WiFI", "WiFi Disconnect from ESP");
        MQTT::getInstance()->wifiDisconnected();
        break;
    
    case WIFI_EVENT_STA_WPS_ER_SUCCESS:
    {
        ESP_LOGI("WiFi", "WIFI_EVENT_STA_WPS_ER_SUCCESS");    
        wifi_event_sta_wps_er_success_t *evt = (wifi_event_sta_wps_er_success_t *) event_data;

        ESP_LOGI ("WiFi", "evt (%d)", (int)evt);
        if (evt) 
        {
            if ( evt->ap_cred_cnt > 1 )
            {
                ESP_LOGW ("WiFi", "WPS received many networks (%d), using first", evt->ap_cred_cnt );
            }      
        
            WiFi::getInstance()->storeWPSAccess( evt->ap_cred[0].ssid, 
                                            evt->ap_cred[0].passphrase,
                                            sizeof(evt->ap_cred[0].ssid),
                                            sizeof(evt->ap_cred[0].passphrase) );
        }
        else
        {   
            wifi_config_t cfg;
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            WiFi::getInstance()->storeWPSAccess( cfg.sta.ssid, 
                                            cfg.sta.password, 
                                            sizeof(cfg.sta.ssid), 
                                            sizeof(cfg.sta.password) );
        }
 
        esp_restart();
        break;
    }
    case WIFI_EVENT_STA_WPS_ER_FAILED:
        ESP_LOGI("WiFi", "WIFI_EVENT_STA_WPS_ER_FAILED");
        WiFi::getInstance()->eraseWPS();
        esp_restart();
        break;
    
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_LOGI("WiFi", "WIFI_EVENT_STA_WPS_ER_TIMEOUT");
        WiFi::getInstance()->eraseWPS();
        esp_restart();
        break;

    default:
        break;
    }
}

void WiFi::eraseWPS()
{
    ESP_LOGI ("WiFi", "Erasing WPS" );
    dbHandle->erase_item("wifi_wps_ssid");
    dbHandle->erase_item("wifi_wps_pass");
    dbHandle->set_item("wps_procedure", false);
    ESP_ERROR_CHECK(dbHandle->commit());
}

void WiFi::storeWPSAccess(uint8_t ssid[], uint8_t password[], int lenSSID, int lenPass)
{
    ESP_LOGI ("WiFi", "WPS SSID: (%s)", ssid );
    ESP_LOGI ("WiFi", "WPS PASS: (%s)", password );
    ESP_ERROR_CHECK(dbHandle->set_blob("wifi_wps_ssid", ssid, lenSSID ));
    ESP_ERROR_CHECK(dbHandle->set_blob("wifi_wps_pass", password, lenPass ));
    ESP_ERROR_CHECK(dbHandle->set_item("wps_procedure", false) );
    ESP_ERROR_CHECK(dbHandle->commit());
}


static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWokenByPost;
    if (WiFi::interruptMr != NULL && WiFi::getInstance() != NULL)
    {
        xQueueSendFromISR(WiFi::getInstance()->mrQueue,
                          &(WiFi::interruptMr),
                          &xHigherPriorityTaskWokenByPost);
    }
}

WiFi *WiFi::instance = NULL;
MRequest *WiFi::interruptMr = NULL;

WiFi::WiFi() : ActiveObject("WiFi", 3000, 7)
{
    ESP_LOGI("WiFi", "init");

    instance = this;
    alt = false;
    wpsProcedure = false;

    initWiFiStorage();
    initWPSButton();
    initWiFiDriver();
    connect_wifi();
}

void WiFi::initWiFiDriver()
{
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &gotIPEventHandler, NULL));
    
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}

void WiFi::initWiFiStorage()
{
    esp_err_t err;
    this->dbHandle = 
        nvs::open_nvs_handle("wifi_storage", NVS_READWRITE, &err);
    
    if (err != ESP_OK) 
    {
        ESP_LOGE("WiFi","Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        wpsProcedure = false;
    } 
    else 
    {
        ESP_LOGI("WiFi","Reading wps from NVS wifi_wps ... ");
        err = dbHandle->get_item("wps_procedure", wpsProcedure);
        switch (err) 
        {
            case ESP_OK:
                ESP_LOGI("WiFi", "... Done");
                ESP_LOGI("WiFi", "wps_procedure = %d", wpsProcedure);
                break;
            default :
                ESP_LOGI("WiFi", "No wifi_wps_procedure in nvs (%s)", esp_err_to_name(err));
                wpsProcedure = 0;
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
    gpio_isr_handler_remove((gpio_num_t) GPIO_INPUT_IO_22);
    wpsProcedure = true;
    
    ESP_LOGI("WiFi","Committing updates in NVS ... ");
    ESP_ERROR_CHECK(dbHandle->set_item("wps_procedure", wpsProcedure) );
    ESP_ERROR_CHECK( dbHandle->commit() );
    esp_restart();
}

void WiFi::connect_alt_wifi(void)
{

    if (wpsProcedure)
    {
        ESP_LOGI("WiFi", "Wps in progress");
        return;
    }

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

    ESP_LOGI("WiFi", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WiFi::connect_wifi(void)
{

    if (wpsProcedure)
    {
        esp_wps_config_t wps_config = {
            .wps_type = WPS_TYPE_PBC ,
            .factory_info = { "ESPRESSIF", "ESP32", "ESPRESSIF IOT", "WWC_DEVICE" }};

        ESP_LOGI("WiFi", "Starting wps");
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_wps_enable( &wps_config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));

        LEDService::getInstance()->wpsInProgress();
    }
    else
    {

        wifi_config_t wifi_config = {.sta =  {
                                        CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD,
                                        WIFI_FAST_SCAN, false,
                                        {'0', '0', '0', '0', '0', '0'},
                                        0, 0,
                                        WIFI_CONNECT_AP_BY_SIGNAL,
                                        {0, WIFI_AUTH_OPEN},
                                        {false, false},
                                        0, 0, 0 } };


        if (dbHandle->get_blob("wifi_wps_ssid", wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid)) == ESP_OK && 
            dbHandle->get_blob("wifi_wps_pass", wifi_config.sta.password, sizeof(wifi_config.sta.password)) == ESP_OK)
        {
            ESP_LOGI("WiFi", "Succesfully read wps ssid and password");
            ESP_LOGI("WiFi", "ssid: %s", wifi_config.sta.ssid);
            ESP_LOGI("WiFi", "pass: %s", wifi_config.sta.password);
        }

        ESP_LOGI("WIFI", "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

    }

}

void WiFi::disconnect(void)
{
    if (!wpsProcedure)
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
    }
}

void WiFi::connectAndToggle(void)
{
    auto f = [=]
    {
        if (!wpsProcedure)
        {
            (alt) ? connect_alt_wifi() : connect_wifi();
            alt = !alt;
        }
    };
    
    executeMethod(f);
}


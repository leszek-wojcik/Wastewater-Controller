#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"
#include "LEDService.h"

WWC *aWWC;
WiFi *aWiFi;
MQTT *aMQTT;
LEDService *aLEDS;

void createActiveObjects()
{
    aLEDS = new LEDService();
    aWiFi = new WiFi();
    aMQTT = new MQTT(aWiFi);
    aWWC = new WWC(aMQTT);
}

extern "C" 
{
    void app_main()
    {
        //Initialize NVS.
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        createActiveObjects();

    }
}

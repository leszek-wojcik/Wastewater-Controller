#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"

extern "C" void app_main();

WWC *aWWC;
WiFi *aWiFi;
MQTT *aMQTT;


void createActiveObjects()
{
    aWWC = new WWC();
    aWiFi = new WiFi();
    aMQTT = new MQTT();
}

void aws_iot_task(void *param) 
{
    aMQTT->mainLoop();
}

void app_main()
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    createActiveObjects();
    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 9216, NULL, 5, NULL, 1);
}

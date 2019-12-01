#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "wwc.h"
#include "wifi.h"
#include "stdio.h"

WiFi::WiFi():ActiveObject("WiFi")
{
    ESP_LOGI("WiFi", "init");
}

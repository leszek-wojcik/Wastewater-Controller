#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "wwc.h"

#include "stdio.h"

#define BLINK_GPIO (gpio_num_t)2

WWC::WWC():ActiveObject("WWC")
{
    ESP_LOGI("WWC", "init");

    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    aWWCtmr =  createTimer (
                [=] () { this->controlOnTmr(); },
                5000/portTICK_PERIOD_MS  );
}

void WWC::controlOnTmr()
{

    printf("hello from WWC\n");
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

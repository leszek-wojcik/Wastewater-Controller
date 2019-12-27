#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"


#include "ActiveObject.h"
#include "wwc.h"
#include "wifi.h"
#include "stdio.h"
#include "mqtt.h"

#include "cJSON.h"

#define BLINK_GPIO (gpio_num_t)2

WWC::WWC():ActiveObject("WWC",2048,6)
{
    ESP_LOGI("WWC", "init");

    wwcCounter = 0;
    areation = false;
    circulation = false;

    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    aWWCtmr = NULL;
    aLEDtmr = NULL;

    ledOn = false;

    createTimer (
            &aWWCtmr,
            [=] () { this->controlOnTmr(); },
            600000/portTICK_PERIOD_MS  ); //10 minutes

    createTimer (
            &aLEDtmr,
            [=] () { this->ledOnTmr(); },
            1500/portTICK_PERIOD_MS  ); //10 minutes

    auto f = [] ( int len, char *s )
    {
        printf ("%d %s",len,s);
    };

    MQTT::getInstance()->subscribeTopic("ala",f);
}

void WWC::ledOnTmr()
{
    if (MQTT::getInstance()->isConnected())
    {
        if (ledOn == true)
        {
            return;
        }

        gpio_set_level(BLINK_GPIO, 1);
        ledOn = true;
    }
    else
    {
        if (ledOn == true)
        {
            gpio_set_level(BLINK_GPIO, 0);
            ledOn = false;
        }
        else
        {
            gpio_set_level(BLINK_GPIO, 1);
            ledOn = true;
        }
       
    }

}


void WWC::controlOnTmr()
{
    
    disableCirculation();
    disableAreation();
    
    if (wwcCounter >= 144)
    {
        wwcCounter = 0;
    }

    if (wwcCounter == 0)
    {
        enableCirculation();
    }

    if ( wwcCounter % 3 )
    {
        enableAreation();
    }

    wwcCounter++; 

    updateControlPins();

}

void WWC::updateControlPins()
{

    cJSON *json = NULL;

    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "name", cJSON_CreateString(CONFIG_AWS_CLIENT_ID));
    cJSON_AddBoolToObject(json, "areation", areation);
    cJSON_AddBoolToObject(json, "circulation", circulation);
    cJSON_AddNumberToObject(json, "wwcCounter", wwcCounter);
    MQTT::getInstance()->sendMQTTmsg(json);
}


#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "wwc.h"
#include "wifi.h"
#include "stdio.h"

#include <chrono>

#include "cJSON.h"
#include "topics.h"

#define BLINK_GPIO (gpio_num_t)2



WWC::WWC(MQTT *mqtt):ActiveObject("WWC",2048,6)
{
    ESP_LOGI("WWC", "init");

    mqttService = mqtt;
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
//            20000/portTICK_PERIOD_MS  ); //20 second
            600000/portTICK_PERIOD_MS  ); //10 minutes

    createTimer (
            &aLEDtmr,
            [=] () { this->ledOnTmr(); },
            1500/portTICK_PERIOD_MS  );


    mqttMsg.reset(new string(""));
    mqttControlTopic.reset(new string(CONTROL_TOPIC));
    mqttStatusTopic.reset(new string(STATUS_TOPIC));

    MqttTopicCallback_t controlTopicCallback = [=] ( MqttMessage_t s )
    {
        executeMethod ([=](){this->onControlTopic(s);});
    };

    mqttService->subscribeTopic(mqttControlTopic,controlTopicCallback);
    mqttService->registerStateCallback( 
        [=](bool connected,bool timeUpdated) { onMqttCallback(connected, timeUpdated);}       
        );
}

void WWC::onMqttCallback( bool connected, bool timeUpdated)
{
    if (timeUpdated)
    {
        executeMethod([](){printf("timeUpdated\n");});
    }
}



void WWC::onControlTopic(MqttMessage_t s)
{
        cJSON *item;
        cJSON *json = cJSON_Parse(s->c_str());

        printf ("onControlTopic inside WWC %s\n",cJSON_Print(json));
        item = cJSON_GetObjectItem(json,"areation");
        if ( item != NULL )
        {
            if (cJSON_IsTrue(item))
            {
                enableAreation();
            }
            if (cJSON_IsFalse(item))
            {
                disableAreation();
            }
        }

        item = cJSON_GetObjectItem(json,"circulation");
        if ( item != NULL )
        {
            if (cJSON_IsTrue(item))
            {
                enableCirculation();
            }
            if (cJSON_IsFalse(item))
            {
                disableCirculation();
            }
        }

        item = cJSON_GetObjectItem(json,"wwcCounter");
        if ( item != NULL )
        {
            if (cJSON_IsNumber(item))
            {
                printf("adjusted wwcCounter to %d\n", item->valueint);
                wwcCounter = item->valueint;
            }
        }

        item = cJSON_GetObjectItem(json,"requestStatus");
        if ( item != NULL )
        {
            if (cJSON_IsTrue(item))
            {
                sendStatus();
            }
        }

        cJSON_Delete(json);
}



void WWC::onStatusTopic(MqttMessage_t s)
{
    printf ("onStatusTopic inside WWC %s\n",s->c_str());
}

void WWC::ledOnTmr()
{
    if (mqttService->isConnected())
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
    sendStatus();
}

void WWC::updateControlPins()
{
    //TBD
}

void WWC::sendStatus()
{
    cJSON *json = NULL;
    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "name", cJSON_CreateString(CONFIG_AWS_CLIENT_ID));
    cJSON_AddBoolToObject(json, "areation", areation);
    cJSON_AddBoolToObject(json, "circulation", circulation);
    cJSON_AddNumberToObject(json, "wwcCounter", wwcCounter);
   
    mqttMsg.reset ( new string( cJSON_Print(json))  );
    mqttService->subjectSend(mqttStatusTopic, mqttMsg);
    cJSON_Delete(json);
}



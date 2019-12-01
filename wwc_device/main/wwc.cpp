#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "wwc.h"
#include "wifi.h"
#include "stdio.h"

#include "cJSON.h"

#define BLINK_GPIO (gpio_num_t)2

WWC::WWC():ActiveObject("WWC",2048,6)
{
    ESP_LOGI("WWC", "init");

    areation = false;
    circulation = false;

    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    aWWCtmr =  createTimer (
                [=] () { this->controlOnTmr(); },
//                600000/portTICK_PERIOD_MS  ); //10 minutes
               5000/portTICK_PERIOD_MS  ); //5 sec
}

extern uint8_t sendMQTTmsg(cJSON *);


void WWC::controlOnTmr()
{

    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    disableCirculation();
    disableAreation();
    
    if (wwcCounter == 144)
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
    //std::string *msg = new std::string("wwcCont ");
    //msg->append(std::to_string(wwcCounter));
    //msg->append(" update control pins:");

    //if (areation) {msg->append(" areation");}
    //if (circulation) {msg->append(" circulation");}

    cJSON *json = NULL;

    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "name", cJSON_CreateString(CONFIG_AWS_CLIENT_ID));
    cJSON_AddBoolToObject(json, "areation", areation);
    cJSON_AddBoolToObject(json, "circulation", circulation);
    cJSON_AddNumberToObject(json, "wwcCounter", wwcCounter);

  //  printf("json:\n%s",cJSON_Print(json));
    sendMQTTmsg(json);
}


#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"

#include "ActiveObject.h"
#include "wwc.h"
#include "wifi.h"
#include "stdio.h"

#include <chrono>

#include "cJSON.h"
#include "topics.h"
#include "driver/gpio.h"

#include "LEDService.h"

#define AREATION_GPIO (gpio_num_t) 17
#define CIRCULATION_GPIO (gpio_num_t) 16

WWC::WWC(MQTT *mqtt):ActiveObject("WWC",4096,6)
{
    ESP_LOGI("WWC", "init");

    mqttService = mqtt;
    areation = false;
    circulation = false;

    adcService = new ADCService();

    gpio_pad_select_gpio(AREATION_GPIO);
    gpio_set_direction(AREATION_GPIO, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(CIRCULATION_GPIO);
    gpio_set_direction(CIRCULATION_GPIO, GPIO_MODE_OUTPUT);

    normalPeriod = 20 * 60 * 1000;
    normalDutyCycle = 0.50;
    circulationStartGMTOffset = -60 * 60 * 1000;
    circulationPeriod = 10 * 60 * 1000;

    pumpFailureThreshold = 20;
    areationPumpReadout = 0;
    circulationPumpReadout = 0;

    sameAsPrevious = 0;

    reportStatusTmr = NULL;
    createTimer (
            &reportStatusTmr,
            [=] () { this->onReportStatus(); },
            1*60000); //5 minute

    createTimer (
        &aControlTmr,
        [=] () { this->controlOnTmr();},
        30000); //0.5 minute

    debugTmr = NULL;
    createTimer (
            &debugTmr,
            [=] () { this->onDebugTmr(); },
            2000);

    startTimer(&reportStatusTmr);
    startTimer(&aControlTmr);
    startTimer(&debugTmr);

    mqttMsg.reset(new string(""));
 
    mqttControlTopic.reset(new string(CONTROL_TOPIC));
    mqttStatusTopic.reset(new string(STATUS_TOPIC));
    mqttShadowUpdateTopic.reset(new string(SHADOW_UPDATE_TOPIC));
    mqttShadowGetTopic.reset(new string(SHADOW_TOPIC_GET_ACCEPTED));
    mqttShadowUpdateDeltaTopic.reset(new string(SHADOW_TOPIC_UPDATE_DELTA));

    MqttTopicCallback_t shadowUpdateDeltaCallback = [=] ( MqttMessage_t s )
    {
        executeMethod ([=](){ ESP_LOGI("WWC", "shadow update delta message"); this->onShadowDelta(s);});
    };

    MqttTopicCallback_t shadowGetUpdateCallback = [=] ( MqttMessage_t s )
    {
        executeMethod ([=](){ESP_LOGI("WWC","shadow update get message"); this->onShadowUpdate(s);});
    };

    MqttTopicCallback_t controlTopicCallback = [=] ( MqttMessage_t s )
    {
        executeMethod ([=](){ESP_LOGI("WWC","control topic message"); this->onControl(s);});
    };


    mqttService->subscribeTopic(mqttControlTopic, controlTopicCallback);
    mqttService->subscribeTopic(mqttShadowGetTopic, shadowGetUpdateCallback);
    mqttService->subscribeTopic(mqttShadowUpdateDeltaTopic, shadowUpdateDeltaCallback);

    // Register callback in MQTTService so we update time in WWC. 
    mqttService->registerStateCallback( 
        [=](bool connected,bool timeUpdated) { onMqttCallback(connected, timeUpdated);}       
        );
    
}

void WWC::onMqttCallback( bool connected, bool timeUpdated)
{
    if (timeUpdated)
    {
        executeMethod( [=](){ onTimeUpdate(); } );
    }

    if(connected)
    {
        executeMethod( [=](){ LEDService::getInstance()->mqttConnected();} );
    }
    else
    {
        executeMethod( [=](){ LEDService::getInstance()->mqttDisconnected();} );
    }
    
}

void WWC::onTimeUpdate()
{
    controlOnTmr();
}

void WWC::onReportStatus()
{
    sendStatus();
}

void WWC::controlOnTmr()
{
    auto tp = system_clock::now();
    milliseconds timeFromMidnight;
    milliseconds timeFromCycleStart;
    int currentDayCycle;

    int aMemmoryAvaliable = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
    int aMemoryLoss = memmoryAvaliable - aMemmoryAvaliable;
    printf("Heap avail: %d, change: %d, ", aMemmoryAvaliable, aMemoryLoss ); 

    sameAsPrevious = 0;
    if (memmoryAvaliable == aMemmoryAvaliable)
    {
        sameAsPrevious = 1;
    }
    
    if ( aMemmoryAvaliable < 80000 && sameAsPrevious == 0)
    {
        ESP_LOGE("WWC","we got memory issue to work");
        esp_restart();
    }
        
    memmoryAvaliable = aMemmoryAvaliable;  


    timeFromMidnight =  duration_cast<milliseconds>( tp.time_since_epoch() - milliseconds(circulationStartGMTOffset) ) 
                            % 
                        duration_cast<milliseconds>( hours(24) ); 
    

    timeFromCycleStart = timeFromMidnight % normalPeriod;
    currentDayCycle  = timeFromMidnight.count() / normalPeriod;
    
    printf("Time from 0 AM: %lld minutes, CurrentCycleTime: %lld minutes, Current Day Cycle: %d, ", 
        duration_cast<minutes>(timeFromMidnight).count(), 
        duration_cast<minutes>(timeFromCycleStart).count(),
        currentDayCycle
        );

    // We assess whether we shoudl be running Areation based on duty cyle and current time within current cycle    
    areation = ( timeFromCycleStart.count() < (normalPeriod * normalDutyCycle) );
    
    // We assess whether we shoudl be running Circulation. Circulation runns only at first cycle for limited amount of time    
    circulation = ( (currentDayCycle == 0) && ( timeFromCycleStart.count() < circulationPeriod) );

    readAreationPumpCurrent();
    readCirculationPumpCurrent();
    updateControlPins();
}

void WWC::readAreationPumpCurrent()
{
    adcService->readAdc1(&areationPumpReadout);
    printf ("Areation current: %d, ", areationPumpReadout);
}

void WWC::readCirculationPumpCurrent()
{
    adcService->readAdc2(&circulationPumpReadout);
    printf ("Circulation current: %d, ", circulationPumpReadout);
}

void WWC::updateControlPins()
{
    printf ("A%dC%d\n", areation, circulation);

    areation ? gpio_set_level(AREATION_GPIO,0) : gpio_set_level(AREATION_GPIO,1);
    circulation ? gpio_set_level(CIRCULATION_GPIO,1) : gpio_set_level(CIRCULATION_GPIO,0);
}

void WWC::processReceivedParameters(cJSON *item)
{
    cJSON *subitem = NULL;
    //This method parsess parameters only
    if (item != NULL)
    {
        subitem = cJSON_GetObjectItem(item, "normalPeriod");
        if (subitem != NULL)
        {
            if (cJSON_IsNumber(subitem))
            {
                if (subitem->valueint > 0)
                {
                    normalPeriod = subitem->valueint;
                }
            }
        }
        subitem = cJSON_GetObjectItem(item, "normalDutyCycle");
        if (subitem != NULL)
        {
            if (cJSON_IsNumber(subitem))
            {
                if (subitem->valuedouble >= 0)
                {
                    normalDutyCycle = subitem->valuedouble;
                }
            }
        }
        subitem = cJSON_GetObjectItem(item, "circulationPeriod");
        if (subitem != NULL)
        {
            if (cJSON_IsNumber(subitem))
            {
                if (subitem->valueint >= 0)
                {
                    circulationPeriod = subitem->valueint;
                }
            }
        }
        subitem = cJSON_GetObjectItem(item, "circulationStartGMTOffset");
        if (subitem != NULL)
        {
            if (cJSON_IsNumber(subitem))
            {
                circulationStartGMTOffset = subitem->valueint;
            }
        }
        subitem = cJSON_GetObjectItem(item, "pumpFailureThreshold");
        if (subitem != NULL)
        {
            if (cJSON_IsNumber(subitem))
            {
                if (subitem->valueint >= 0)
                {
                    pumpFailureThreshold = subitem->valueint;
                }
            }
        }
    }
}

void WWC::onShadowUpdate(MqttMessage_t s)
{
    cJSON *docitem = NULL;
    cJSON *item = NULL;
    cJSON *json = cJSON_Parse(s->c_str());
    if (json != NULL)
    {
        docitem = cJSON_GetObjectItem(json, "state");
        if (docitem != NULL)
        {
            item = cJSON_GetObjectItem(docitem, "desired");
            processReceivedParameters(item);
        }
    }
    cJSON_Delete(json);
    sendShadow();
}

void WWC::onShadowDelta(MqttMessage_t s)
{
    cJSON *docitem = NULL;
    cJSON *json = cJSON_Parse(s->c_str());
    if (json != NULL)
    {
        docitem = cJSON_GetObjectItem(json, "state");
        if (docitem != NULL)
        {
            processReceivedParameters(docitem);
        }
    }
    cJSON_Delete(json);
    sendShadow();
}

void WWC::onControl(MqttMessage_t s)
{
    cJSON *docitem = NULL;
    cJSON *json = cJSON_Parse(s->c_str());
    if (json != NULL)
    {
        docitem = cJSON_GetObjectItem(json, "state");
        if (docitem != NULL)
        {
            processReceivedParameters(docitem);
        }
    }
    cJSON_Delete(json);
    sendStatus();
}

void WWC::sendShadow()
{   
    cJSON *jsondoc = cJSON_CreateObject();
    cJSON *jsonState = cJSON_CreateObject();
    cJSON *jsonReported = cJSON_CreateObject();

    // Configuration items
    cJSON_AddNumberToObject(jsonReported, "normalPeriod", normalPeriod);
    cJSON_AddNumberToObject(jsonReported, "circulationStartGMTOffset", circulationStartGMTOffset);
    cJSON_AddNumberToObject(jsonReported, "circulationPeriod", circulationPeriod );
    cJSON_AddNumberToObject(jsonReported, "normalDutyCycle", normalDutyCycle );
    cJSON_AddNumberToObject(jsonReported, "pumpFailureThreshold", pumpFailureThreshold);
        
    cJSON_AddItemToObject(jsonState, "reported",jsonReported );
    cJSON_AddItemToObject(jsondoc,"state",jsonState);

    char *str = cJSON_PrintUnformatted(jsondoc);
    mqttMsg.reset ( new string( str )  );
    free (str);

    mqttService->subjectSend(mqttShadowUpdateTopic, mqttMsg);
    cJSON_Delete(jsondoc);
}

void WWC::sendStatus()
{   
    cJSON *jsondoc = cJSON_CreateObject();
    cJSON *jsonState = cJSON_CreateObject();
    cJSON *jsonReported = cJSON_CreateObject();

    cJSON_AddNumberToObject(jsonReported, "areationPumpReadout", areationPumpReadout);
    cJSON_AddNumberToObject(jsonReported, "circulationPumpReadout", circulationPumpReadout);   
    cJSON_AddBoolToObject(jsonReported, "areation", areation);
    cJSON_AddBoolToObject(jsonReported, "circulation", circulation);
    
    cJSON_AddItemToObject(jsonState, "reported",jsonReported );
    cJSON_AddItemToObject(jsondoc,"state",jsonState);

    char *str = cJSON_PrintUnformatted(jsondoc);
    mqttMsg.reset ( new string( str )  );
    free (str);

    mqttService->subjectSend(mqttStatusTopic, mqttMsg);
    cJSON_Delete(jsondoc);
}

void WWC::onDebugTmr()
{
    //adcService->readAdc1();
}

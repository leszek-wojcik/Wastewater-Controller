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
#include <driver/adc.h>

#define BLINK_GPIO (gpio_num_t)2
#define AREATION_GPIO (gpio_num_t) 17
#define CIRCULATION_GPIO (gpio_num_t) 16


WWC::WWC(MQTT *mqtt):ActiveObject("WWC",2048,6)
{
    ESP_LOGI("WWC", "init");

    mqttService = mqtt;
    areation = false;
    circulation = false;

    gpio_pad_select_gpio(AREATION_GPIO);
    gpio_set_direction(AREATION_GPIO, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(CIRCULATION_GPIO);
    gpio_set_direction(CIRCULATION_GPIO, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_0);

    cycleTmrValue = 24 * 60 * 60 *1000; //24 hours
    areationOnTmrValue =  90 * 60 *1000; //90 minutes
    areationOffTmrValue = 30 * 60 *1000; //30 minutes
    circulationOnTmrValue = 10 * 60 *1000; // 10 minutes

    // this coresponds to above
    normalPeriod = 120 * 60 * 1000;
    normalDutyCycle = 0.75;
    circulationStartGMTOffset = -60 * 60 * 1000;

    pumpFailureThreshold = 20;
    areationPumpFailureReadCount = 0;
    circulationPumpFailureReadCount = 0;
    areationPumpReadout = 0;
    circulationPumpReadout = 0;

    sameAsPrevious = 0;

    reportStatusTmr = NULL;
    createTimer (
            &reportStatusTmr,
            [=] () { this->controlOnTmr(); },
            60000); //1 minute

    aLEDtmr = NULL;
    createTimer (
            &aLEDtmr,
            [=] () { this->ledOnTmr(); },
            1500);

    areationOnTmr = NULL;
    createTimer( &areationOnTmr,
                        [=]() {onAreationOnTmrExpiry();},
                        areationOnTmrValue );

    areationOffTmr = NULL;
    createTimer ( &areationOffTmr,
                        [=]() { onAreationOffTmrExpiry();},
                        areationOffTmrValue);

    cycleTmr = NULL;
    createTimer( &cycleTmr,
                [=](){onCycleTmrExpiry();},
                cycleTmrValue);

    circulationOnTmr = NULL;
    createTimer( &circulationOnTmr,
                [=](){ onCirculationOnTmrExpiry(); },
                circulationOnTmrValue );


    ledOn = false;

    startTimer(&reportStatusTmr);

    startTimer(&aLEDtmr);
    
    startCycleTimer();


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


    enableAreation();
    disableCirculation();
    updateControlPins();

    startAreationOnTmr();

}
void WWC::restartCycle(void)
{
    stopAreationOnTmr();
    stopCirculationOnTmr();
    stopAreationOffTmr ();

    disableCirculation();
    enableAreation();
    updateControlPins();

    startAreationOnTmr();
}

void WWC::onMqttCallback( bool connected, bool timeUpdated)
{
    if (timeUpdated)
    {
        executeMethod( [=](){ onTimeUpdate(); } );
    }
}

void WWC::onTimeUpdate()
{
    auto tp = system_clock::now();
    milliseconds timeToMidnight;

    timeToMidnight = duration_cast<milliseconds>(hours(24))
                   - ( duration_cast<milliseconds>(tp.time_since_epoch() -
                     milliseconds(circulationStartGMTOffset)) %
                        duration_cast<milliseconds>(hours(24))); 
                    

    printf("Time to midnight: %lld minutes\n", duration_cast<minutes>(timeToMidnight).count() );


    if ( !isTimerActive(&circulationOnTmr) )
    {
        cycleTmrValue = timeToMidnight.count();
        stopCycleTimer();
        startCycleTimer();
        printf ("setting cycle timer %lld minutes from now\n", 
                duration_cast<minutes>(timeToMidnight).count()); 
    }  
    else
    {
        printf ("skipping cycle adjustment for sake of successful current cycle\n" );
    }
    

}

void WWC::onControlTopic(MqttMessage_t s)
{
        cJSON *item;
        cJSON *json = cJSON_Parse(s->c_str());
        bool configurationUpdated = false;

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
            updateControlPins();
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
            updateControlPins();
        }

        item = cJSON_GetObjectItem(json,"requestStatus");
        if ( item != NULL )
        {
            if (cJSON_IsTrue(item))
            {
                sendStatus();
            }
        }

        item = cJSON_GetObjectItem(json,"normalPeriod");
        if ( item != NULL )
        {
            if (cJSON_IsNumber(item))
            {
                normalPeriod = item->valueint;
                configurationUpdated = true;
            }
        }

        item = cJSON_GetObjectItem(json,"normalDutyCycle");
        if ( item != NULL )
        {
            if (cJSON_IsNumber(item))
            {
                normalDutyCycle = item->valuedouble;
                configurationUpdated = true;
            }
        }
        
        item = cJSON_GetObjectItem(json,"circulationStartGMTOffset");
        if ( item != NULL )
        {
            if (cJSON_IsNumber(item))
            {
                circulationStartGMTOffset = item->valuedouble;
            }
        }
 
        item = cJSON_GetObjectItem(json,"pumpFailureThreshold");
        if ( item != NULL )
        {
            if (cJSON_IsNumber(item))
            {
                pumpFailureThreshold = item->valuedouble;
            }
        }

        cJSON_Delete(json);

        if (configurationUpdated == true)
        {
            updateConfiguration();
            restartCycle();
        }
}

void WWC::updateConfiguration()
{
    areationOnTmrValue =  normalPeriod * normalDutyCycle;
    areationOffTmrValue = normalPeriod - areationOnTmrValue;
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
    sendStatus();
}

void WWC::updateControlPins()
{
    printf ("A%dC%d\n", areation, circulation);

    areation ? gpio_set_level(AREATION_GPIO,1) : gpio_set_level(AREATION_GPIO,0);
    circulation ? gpio_set_level(CIRCULATION_GPIO,1) : gpio_set_level(CIRCULATION_GPIO,0);

    sendStatus();
}

void WWC::sendStatus()
{
    
    cJSON *json = NULL;
    int aMemmoryAvaliable = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
    int aMemoryLoss = memmoryAvaliable - aMemmoryAvaliable;

    json = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "name", cJSON_CreateString(CONFIG_AWS_CLIENT_ID));
    cJSON_AddBoolToObject(json, "areation", areation);
    cJSON_AddBoolToObject(json, "circulation", circulation);
    cJSON_AddNumberToObject(json, "freeHeap", aMemmoryAvaliable);
    cJSON_AddNumberToObject(json, "areationPumpReadout", areationPumpReadout);
    cJSON_AddNumberToObject(json, "circulationPumpReadout", circulationPumpReadout);
   
    printf("free heap %d\n", aMemmoryAvaliable);
    
    if (memmoryAvaliable == aMemmoryAvaliable)
    {
        sameAsPrevious = 1;
    }

    if (  memmoryAvaliable >  aMemmoryAvaliable )
    {
        printf("memory loss by %d\n", aMemoryLoss  ); 
    }

    if ( aMemmoryAvaliable < 180000 && sameAsPrevious == 1)
    {
        printf("we got issue to work\n");
    }

    memmoryAvaliable = aMemmoryAvaliable;  

    char *str = cJSON_Print(json);
    mqttMsg.reset ( new string( str )  );
    free (str);

    mqttService->subjectSend(mqttStatusTopic, mqttMsg);
    cJSON_Delete(json);
}


void WWC::startAreationOnTmr()
{
    changeTimerPeriod(&areationOnTmr, areationOnTmrValue);
    startTimer(&areationOnTmr);
}

void WWC::onAreationOnTmrExpiry()
{
    printf(__PRETTY_FUNCTION__);
    printf("\n");

    stopAreationOnTmr();
    disableAreation();
    disableCirculation();
    updateControlPins();
    startAreationOffTmr();
}

void WWC::readAreationPumpCurrent()
{
    areationPumpReadout = adc1_get_raw(ADC1_CHANNEL_0);

    printf ("!!!Areation ADC read: %d\n", areationPumpReadout);

    if (areationPumpReadout >= 0 && areationPumpReadout <= pumpFailureThreshold )
    {
        areationPumpFailureReadCount++;
    }
    else
    {
        areationPumpFailureReadCount=0;
    }

    if (areationPumpFailureReadCount == 5)
        printf ("!!!Areation pump failure\n");
    
}

void WWC::readCirculationPumpCurrent()
{
    circulationPumpReadout = adc1_get_raw(ADC1_CHANNEL_3);

    printf ("!!!Circulation ADC read: %d\n", circulationPumpReadout);

    if (circulationPumpReadout >= 0 && circulationPumpReadout <= pumpFailureThreshold)
    {
        circulationPumpFailureReadCount++;
    }
    else
    {
        circulationPumpFailureReadCount=0;
    }

    if (circulationPumpFailureReadCount == 5)
        printf ("!!!circulation pump failure\n");
}

void WWC::stopAreationOnTmr()
{
    stopTimer( &areationOnTmr );
}

void WWC::startAreationOffTmr()
{
    changeTimerPeriod( &areationOffTmr, areationOffTmrValue);
    startTimer ( &areationOffTmr);
}

void WWC::onAreationOffTmrExpiry()
{
    printf(__PRETTY_FUNCTION__);
    printf("\n");
    stopAreationOffTmr();
    enableAreation();
    disableCirculation();
    updateControlPins();
    startAreationOnTmr();
}

void WWC::stopAreationOffTmr()
{
    stopTimer( &areationOffTmr );
}

void WWC::startCycleTimer()
{
    changeTimerPeriod(&cycleTmr, cycleTmrValue);
    startTimer(&cycleTmr);
}

void WWC::onCycleTmrExpiry()
{
    printf(__PRETTY_FUNCTION__);
    printf("\n");

    stopAreationOnTmr();
    stopAreationOffTmr();
    stopCirculationOnTmr();

    disableAreation();
    enableCirculation();
    updateControlPins();

    startCirculationOnTmr();
}

void WWC::stopCycleTimer()
{
    stopTimer( &cycleTmr );
}

void WWC::startCirculationOnTmr()
{
    changeTimerPeriod ( &circulationOnTmr, circulationOnTmrValue );
    startTimer( &circulationOnTmr);
}
void WWC::onCirculationOnTmrExpiry()
{
    printf(__PRETTY_FUNCTION__);
    printf("\n");
    stopCirculationOnTmr();
    disableCirculation();
    enableAreation();
    startAreationOnTmr();
    updateControlPins();
}
void WWC::stopCirculationOnTmr()
{
   stopTimer ( &circulationOnTmr ); 
}

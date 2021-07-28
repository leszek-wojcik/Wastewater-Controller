#include "driver/gpio.h"
#include "LEDService.h"

LEDService *LEDService::instance = NULL;

LEDService::LEDService():ActiveObject("LEDS", 1000, 0)
{
    instance = this;
    ledTmr   = NULL;
    
    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    setCadenceDuty(1000,0.5,0);
}

void LEDService::setCadenceDuty(int cadence, float duty, int count)
{
    maxCadence = count;
    currentDuty = duty;
    currentCadence = 0;
    currentExpiry = 0;

    int tmrValue = cadence * currentDuty;
    maxExpiry = cadence/tmrValue;

    createTimer (
            &ledTmr, 
            [=] () { tmrExpiry(); },
            tmrValue);

    gpio_set_level(BLINK_GPIO, 1);
    startTimer(&ledTmr);
}

void LEDService::tmrExpiry(void)
{
    currentExpiry++;
    if (currentExpiry >= maxExpiry)
    {
        currentExpiry = 0;
        currentCadence++;
        gpio_set_level(BLINK_GPIO, 1);
        if (currentCadence >= maxCadence && maxCadence != 0)
        {
            stopTimer(&ledTmr);
        }
        gpio_set_level(BLINK_GPIO, 1);
    }
    else
    {
        gpio_set_level(BLINK_GPIO, 0);
    }
}

void LEDService::wpsInProgress(void)
{
    executeMethod( [=](){ setCadenceDuty(200, 0.2, 0); } );
}

void LEDService::mqttConnected(void)
{
    executeMethod( [=](){ setCadenceDuty(1000, 1, 0); } );
}

void LEDService::mqttDisconnected(void)
{
    executeMethod( [=](){ setCadenceDuty(1000,0.5,0); } );
}
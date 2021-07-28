#ifndef LEDSERVICE_INCLUDED
#define LEDSERVICE_INCLUDED

#include "ActiveObject.h"

#define BLINK_GPIO (gpio_num_t)2

class LEDService: public ActiveObject
{
    private:
        static LEDService* instance;
        AOTimer_t ledTmr;
        int currentCadence;
        int currentExpiry;
        int maxCadence;
        int maxExpiry;
        float currentDuty;
        void tmrExpiry (void);
        void setCadenceDuty(int cadence, float duty, int count);
    public:
        LEDService();

        static LEDService* getInstance()
        {
            return instance;
        }

        void wpsInProgress(void);
        void mqttConnected(void);
        void mqttDisconnected(void);
};

#endif


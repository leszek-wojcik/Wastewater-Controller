#include "ActiveObject.h"
#include <memory>
#include "mqtt.h"

using namespace std;

class WWC : public ActiveObject
{
    private:
        AOTimer_t reportStatusTmr;
        AOTimer_t aLEDtmr;
        AOTimer_t areationOnTmr;
        AOTimer_t areationOffTmr;
        AOTimer_t cycleTmr;
        AOTimer_t circulationOnTmr;

        AODurationMS_t cycleTmrValue;
        AODurationMS_t areationOnTmrValue;
        AODurationMS_t areationOffTmrValue;
        AODurationMS_t circulationOnTmrValue;


        bool areation;
        bool circulation;
        int wwcCounter;
        bool ledOn;

        MqttTopic_t mqttControlTopic;
        MqttTopic_t mqttStatusTopic;
        MqttMessage_t mqttMsg;
        MQTT* mqttService;

        int     normalPeriod;
        double  normalDutyCycle;

        void updateConfiguration();
        void restartCycle();
        
    public:
        WWC(MQTT *);

        void onControlTopic(MqttMessage_t);
        void onStatusTopic(MqttMessage_t);
        void onMqttCallback( bool connected, bool timeUpdated);

        inline void enableCirculation() { circulation = true; }
        inline void disableCirculation() { circulation = false; }

        inline void enableAreation() { areation = true; }
        inline void disableAreation() { areation = false; }

        void updateControlPins();
        void sendStatus();

        void controlOnTmr();
        void ledOnTmr();

        void startAreationOnTmr();
        void onAreationOnTmrExpiry();
        void stopAreationOnTmr();

        void startAreationOffTmr();
        void onAreationOffTmrExpiry();
        void stopAreationOffTmr();

        void startCycleTimer();
        void onCycleTmrExpiry();
        void stopCycleTimer();

        void startCirculationOnTmr();
        void onCirculationOnTmrExpiry();
        void stopCirculationOnTmr();

};


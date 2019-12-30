#include "ActiveObject.h"
#include <memory>
#include "mqtt.h"

using namespace std;

class WWC : public ActiveObject
{
    private:
        TimerHandle_t aWWCtmr;
        TimerHandle_t aLEDtmr;
        TimerHandle_t backOffTmr;

        bool areation;
        bool circulation;
        int wwcCounter;
        bool ledOn;

        MqttTopic_t mqttControlTopic;
        MqttTopic_t mqttStatusTopic;
        MqttTopic_t mqttTimeTopic;
        MqttTopic_t mqttPingTopic;
        MqttMessage_t mqttMsg;

        void obtainTime(void);
        void onBackOffTmr(void);
        void onTimeTopic(MqttMessage_t);
        
    public:
        WWC();

        void onControlTopic(MqttMessage_t);
        void onStatusTopic(MqttMessage_t);

        inline void enableCirculation() { circulation = true; }
        inline void disableCirculation() { circulation = false; }

        inline void enableAreation() { areation = true; }
        inline void disableAreation() { areation = false; }

        void updateControlPins();
        void sendStatus();

        void controlOnTmr();
        void ledOnTmr();
};


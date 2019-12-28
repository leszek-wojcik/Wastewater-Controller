#include "ActiveObject.h"
#include <memory>
#include "mqtt.h"

using namespace std;

class WWC : public ActiveObject
{
    private:
        TimerHandle_t aWWCtmr;
        TimerHandle_t aLEDtmr;
        bool areation;
        bool circulation;
        int wwcCounter;
        bool ledOn;

        MqttTopic_t mqttTopic;
        MqttTopic_t mqttStatusTopic;
        MqttMessage_t mqttMsg;
        
    public:
        WWC();

        void onControlTopic(MqttMessage_t);
        void onStatusTopic(MqttMessage_t);

        inline void disableCirculation() { circulation = false; }
        inline void enableCirculation() { circulation = true; }
        inline void disableAreation() { areation = false; }
        inline void enableAreation() { areation = true; }

        void updateControlPins();

        void controlOnTmr();
        void ledOnTmr();
};


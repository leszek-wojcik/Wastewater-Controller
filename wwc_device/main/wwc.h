#include "ActiveObject.h"
#include <memory>
#include "mqtt.h"

using namespace std;
using namespace std::chrono;

class WWC : public ActiveObject
{
    private:
        AOTimer_t reportStatusTmr;
        AOTimer_t aLEDtmr;
        AOTimer_t aControlTmr;

        bool areation;
        bool circulation;
        bool ledOn;

        MqttTopic_t mqttControlTopic;
        MqttTopic_t mqttStatusTopic;
        MqttMessage_t mqttMsg;
        MQTT* mqttService;

        // This are ment to be configuration items. 
        // Default values are set in constructor in case of no connectivity
        int     normalPeriod;                   // Defines basic operation window 
        double  normalDutyCycle;                // Defines how long areation pump operates during normal period
        int     circulationStartGMTOffset;      // GMT Offset 
        int     circulationPeriod;              // Defines how long circulation pump operates during day
        int     pumpFailureThreshold;           // Helps to interpret if we have pump failure
        
        int     areationPumpFailureReadCount;
        int     circulationPumpFailureReadCount;
        int     areationPumpReadout;
        int     circulationPumpReadout;

        void onTimeUpdate();

        int memmoryAvaliable;
        int sameAsPrevious; 
        
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
        void onReportStatus();


        void readAreationPumpCurrent();
        void readCirculationPumpCurrent();
};


#include "cJSON.h"
#include "string.h"
#include "aws_iot_mqtt_client_interface.h"
#include "ActiveObject.h"

class MQTT_FSM_State;
class MQTT_Init_State;
class MQTT_Connecting_State;
class MQTT_Connected_State;

class MQTT: public ActiveObject
{
    private:
        static MQTT* instance;
        static MQTT_FSM_State* currentState;
        static MQTT_Init_State* initState;
        static MQTT_Connecting_State* connectingState;
        static MQTT_Connected_State* connectedState;

        char topic[5];
        int topicLen;
        char pingTopic[5];
        int pingTopicLen;

        TimerHandle_t throttleTmr;
        TimerHandle_t pingTmr;
        TimerHandle_t activityTmr;
        bool activityInd;
        int numberOfPings;

    public:
        MQTT():ActiveObject("MQTT", 9216, 5)
        {
            instance = this;
            createStateMachine();

            strcpy(topic,"wwc");
            topicLen = strlen(topic);

            strcpy(pingTopic,"wwcping");
            pingTopicLen = strlen(pingTopic);
            activityInd = false;
            numberOfPings = 0;
        }

        static MQTT* getInstance()
        {
            return instance;
        }

        void initParams();
        void connect();
        void subscribePing();
        void unsubscribePing();
        void subscribeTopic();
        void unsubscribeTopic();

        void createStateMachine();

        void throttle(uint32_t tmo=100);
        void startThrottleTmr();
        void stopThrottleTmr();

        void ping();
        void startPingTmr();
        void stopPingTmr();

        void activity();
        void startActivityTmr();
        void stopActivityTmr();
        void activityPresent();

        AWS_IoT_Client client;
        char cPayload[100];

        IoT_Publish_Message_Params paramsQOS0;
        IoT_Client_Init_Params mqttInitParams;
        IoT_Client_Connect_Params connectParams;

        uint8_t sendMQTTmsg(cJSON *s);

        // State Machine 
        void wifiConnected();
        void wifiDisconnected();
        void established();

        friend class MQTT_FSM_State;
        friend class MQTT_Init_State;
        friend class MQTT_Connecting_State;
        friend class MQTT_Connected_State;

};



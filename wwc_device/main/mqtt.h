#include "cJSON.h"
#include "string.h"
#include "aws_iot_mqtt_client_interface.h"
#include "ActiveObject.h"


class WiFi;
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
        TimerHandle_t initTmr;
        TimerHandle_t reconnectTmr;
        TimerHandle_t safeGuardTmr;

        bool activityInd;
        WiFi *wifi;

    public:
        MQTT(WiFi *wifi);

        static MQTT* getInstance()
        {
            return instance;
        }

        void initParams();

        void connectWiFi();
        void toggleWiFi();
        void disconnectWiFi();

        bool connectMQTT();
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

        void safeGuard();
        void startSafeGuardTmr();
        void stopSafeGuardTmr();

        void init();
        void startInitTmr();
        void stopInitTmr();

        void reconnectMQTT();
        void startReconnectTmr();
        void stopReconnectTmr();


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
        void onError();

        friend class MQTT_FSM_State;
        friend class MQTT_Init_State;
        friend class MQTT_Connecting_State;
        friend class MQTT_Connected_State;

};



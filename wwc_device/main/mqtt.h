#ifndef MQTT_INCLUDED
#define MQTT_INCLUDED

#include "cJSON.h"
#include "aws_iot_mqtt_client_interface.h"
#include "ActiveObject.h"
#include <string>
#include <memory>

using namespace std;

class WiFi;
class MQTT_FSM_State;
class MQTT_Init_State;
class MQTT_Connecting_State;
class MQTT_Connected_State;

typedef shared_ptr<string> MqttTopic_t;
typedef shared_ptr<string> MqttMessage_t;
typedef function<void(MqttMessage_t)> MqttTopicCallback_t;

class MQTT: public ActiveObject
{
    private:
        static MQTT* instance;
        static MQTT_FSM_State* currentState;
        static MQTT_Init_State* initState;
        static MQTT_Connecting_State* connectingState;
        static MQTT_Connected_State* connectedState;

        char topic[20];
        int topicLen;
        char pingTopic[20];
        int pingTopicLen;
        char cPayload[100];

        IoT_Publish_Message_Params paramsQOS0;
        IoT_Client_Init_Params mqttInitParams;
        IoT_Client_Connect_Params connectParams;
        AWS_IoT_Client client;

        TimerHandle_t throttleTmr;
        TimerHandle_t pingTmr;
        TimerHandle_t activityTmr;
        TimerHandle_t initTmr;
        TimerHandle_t reconnectTmr;
        TimerHandle_t safeGuardTmr;

        bool activityInd;
        WiFi *wifi;

        void initParams();

        void connectWiFi();
        void toggleWiFi();
        void disconnectWiFi();

        bool connectMQTT();
        void subscribePing();
        void unsubscribePing();

        void subscribeTopics();
        void unsubscribeTopics();
        void executeSubscribeTopic(MqttTopic_t);
        void executeUnsubscribeTopic(MqttTopic_t);
        void addToSubscriptions(MqttTopic_t, MqttTopicCallback_t );
        void removeFromSubscriptions(MqttTopic_t);
        map<MqttTopic_t, MqttTopicCallback_t > subscriptions;


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

        uint8_t answerPing();


        void sendMQTTmsg(MqttTopic_t, MqttMessage_t );
        void processIncommingMessage(MqttTopic_t, MqttMessage_t);

    public:
        MQTT(WiFi *wifi);

        static MQTT* getInstance()
        {
            return instance;
        }

        void dispatchMessage(MqttTopic_t topic, MqttMessage_t msg);

        // State Machine 
        void wifiConnected();
        void wifiDisconnected();
        void pingReceived();
        void onError();
        void subscribeTopic(MqttTopic_t, MqttTopicCallback_t );
        void subjectSend(MqttTopic_t, MqttMessage_t);
        void unsubscribeTopic(MqttTopic_t);
        bool isConnected(); 

        friend class MQTT_FSM_State;
        friend class MQTT_Init_State;
        friend class MQTT_Connecting_State;
        friend class MQTT_Connected_State;

};

#endif


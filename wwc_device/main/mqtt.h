#ifndef MQTT_INCLUDED
#define MQTT_INCLUDED

#include "cJSON.h"
#include "aws_iot_mqtt_client_interface.h"
#include "ActiveObject.h"
#include <string.h>
#include <string>
#include <chrono>
#include <memory>

using namespace std;
using namespace std::chrono;

class WiFi;
class MQTT_FSM_State;
class MQTT_Init_State;
class MQTT_Connecting_State;
class MQTT_Connected_State;

typedef shared_ptr<string> MqttTopic_t;
typedef shared_ptr<string> MqttMessage_t;
typedef function<void(MqttMessage_t)> MqttTopicCallback_t;
typedef function<void(bool,bool)> MqttStateCallback_t;

class MQTT: public ActiveObject
{
    private:
        static MQTT* instance;
        static MQTT_FSM_State* currentState;
        static MQTT_Init_State* initState;
        static MQTT_Connecting_State* connectingState;
        static MQTT_Connected_State* connectedState;

        MqttTopic_t mqttTimeTopic;

        char cPayload[100];

        IoT_Publish_Message_Params paramsQOS0;
        IoT_Client_Init_Params mqttInitParams;
        IoT_Client_Connect_Params connectParams;
        AWS_IoT_Client client;

        AOTimer_t throttleTmr;
        AOTimer_t obtainTimeTmr;
        AOTimer_t activityTmr;
        AOTimer_t initTmr;
        AOTimer_t reconnectTmr;
        AOTimer_t safeGuardTmr;

        MqttStateCallback_t stateCallback;

        bool activityInd;
        WiFi *wifi;

        void initParams();

        void connectWiFi();
        void toggleWiFi();
        void disconnectWiFi();

        bool connectMQTT();
        void executeDisconnect();
        void executeSubscribeTime();
        void executeUnsubscribeTime();

        void subscribeTopics();
        void unsubscribeTopics();
        void executeSubscribeTopic(MqttTopic_t);
        void executeUnsubscribeTopic(MqttTopic_t);
        void executeSubjectSend(MqttTopic_t, MqttMessage_t );
        void addToSubscriptions(MqttTopic_t, MqttTopicCallback_t );
        void removeFromSubscriptions(MqttTopic_t);
        map<MqttTopic_t, MqttTopicCallback_t > subscriptions;


        void createStateMachine();

        void throttle(uint32_t tmo=100);
        void startThrottleTmr();
        void stopThrottleTmr();

        void obtainTime();
        void startObtainTimeTmr();
        void stopObtainTimeTmr();
        void processTimeMessage( MqttMessage_t msg);

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

        void runStateCallback(bool, bool);

        void processIncommingMessage(MqttTopic_t, MqttMessage_t);

    public:
        MQTT(WiFi *wifi);

        static MQTT* getInstance()
        {
            return instance;
        }

        void dispatchMessage(MqttTopic_t topic, MqttMessage_t msg);
        void registerStateCallback(MqttStateCallback_t);

        // State Machine 
        void wifiConnected();
        void wifiDisconnected();
        void timeReceived(MqttMessage_t);
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


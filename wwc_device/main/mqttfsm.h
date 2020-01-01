using namespace std;
class MQTT;

class MQTT_FSM_State
{
    protected:
        MQTT *context;
    public:
        MQTT_FSM_State(MQTT *ctx):context(ctx){};
        void stateTransition(MQTT_FSM_State *next);
        virtual void wifiConnected() = 0;
        virtual void wifiDisconnected() = 0;
        virtual void timeReceived(MqttMessage_t) = 0;
        virtual void subscribeTopic(MqttTopic_t, MqttTopicCallback_t) = 0;
        virtual void subjectSend(MqttTopic_t, MqttMessage_t) = 0;
        virtual void unsubscribeTopic(MqttTopic_t) = 0;
        virtual void onEntry() = 0;
        virtual void onExit()  = 0;
        virtual void onError() = 0;
};

class MQTT_Init_State: public MQTT_FSM_State
{
    public:
        MQTT_Init_State(MQTT *ctx):MQTT_FSM_State(ctx){};
        void wifiConnected() override;
        void wifiDisconnected() override; 
        void timeReceived(MqttMessage_t) override;
        void subscribeTopic(MqttTopic_t, MqttTopicCallback_t) override;
        void subjectSend(MqttTopic_t, MqttMessage_t) override;
        void unsubscribeTopic(MqttTopic_t) override ;
        void onEntry() override;
        void onExit() override;
        void onError() override;
};
 
class MQTT_Connecting_State: public MQTT_FSM_State
{
    public:
        MQTT_Connecting_State(MQTT *ctx):MQTT_FSM_State(ctx){};
        void wifiConnected() override;
        void wifiDisconnected() override; 
        void timeReceived(MqttMessage_t) override;
        void subscribeTopic(MqttTopic_t, MqttTopicCallback_t) override;
        void subjectSend(MqttTopic_t, MqttMessage_t) override;
        void unsubscribeTopic(MqttTopic_t) override ;
        void onEntry() override;
        void onExit() override;
        void onError() override;
};

class MQTT_Connected_State: public MQTT_FSM_State
{
    public:
        MQTT_Connected_State(MQTT *ctx):MQTT_FSM_State(ctx){};
        void wifiConnected() override;
        void wifiDisconnected() override; 
        void timeReceived(MqttMessage_t) override;
        void subscribeTopic(MqttTopic_t, MqttTopicCallback_t) override;
        void subjectSend(MqttTopic_t, MqttMessage_t) override;
        void unsubscribeTopic(MqttTopic_t) override ;
        void onEntry() override;
        void onExit() override;
        void onError() override;
};
 


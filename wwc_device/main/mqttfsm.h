class MQTT;

class MQTT_FSM_State
{
    protected:
        MQTT *context;
    public:
        MQTT_FSM_State(MQTT *ctx):context(ctx){};
        void stateTransition(MQTT_FSM_State *next);
        virtual void wifiConnected() = 0;
        virtual void wifiDisconnected() =0;
        virtual void pingReceived() =0;
        virtual void subscribeTopic(std::string, std::function<void(int,char*)>) =0;
        virtual void onEntry() =0;
        virtual void onExit() =0;
        virtual void onError() = 0;
};

class MQTT_Init_State: public MQTT_FSM_State
{
    public:
        MQTT_Init_State(MQTT *ctx):MQTT_FSM_State(ctx){};
        void wifiConnected() override;
        void wifiDisconnected() override; 
        void pingReceived() override;
        void subscribeTopic(std::string, std::function<void(int,char*)>) override;
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
        void pingReceived() override;
        void subscribeTopic(std::string, std::function<void(int,char*)>) override;
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
        void pingReceived() override;
        void subscribeTopic(std::string, std::function<void(int,char*)>) override;
        void onEntry() override;
        void onExit() override;
        void onError() override;
};
 


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
        virtual void established() =0;
        virtual void onEntry() =0;
        virtual void onExit() =0;
};

class MQTT_Init_State: public MQTT_FSM_State
{
    public:
        MQTT_Init_State(MQTT *ctx):MQTT_FSM_State(ctx){};
        void wifiConnected() override;
        void wifiDisconnected() override; 
        void established() override;
        void onEntry() override;
        void onExit() override;
};
 
class MQTT_Connecting_State: public MQTT_FSM_State
{
    public:
        MQTT_Connecting_State(MQTT *ctx):MQTT_FSM_State(ctx){};
        void wifiConnected() override;
        void wifiDisconnected() override; 
        void established() override;
        void onEntry() override;
        void onExit() override;
};

class MQTT_Connected_State: public MQTT_FSM_State
{
    public:
        MQTT_Connected_State(MQTT *ctx):MQTT_FSM_State(ctx){};
        void wifiConnected() override;
        void wifiDisconnected() override; 
        void established() override;
        void onEntry() override;
        void onExit() override;
};
 


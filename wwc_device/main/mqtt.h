#include "cJSON.h"
#include "aws_iot_mqtt_client_interface.h"
#include "ActiveObject.h"

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
 

class MQTT: public ActiveObject
{
    private:
        static MQTT* instance;
        static MQTT_FSM_State* currentState;
        static MQTT_Init_State* initState;
        static MQTT_Connecting_State* connectingState;
        static MQTT_Connected_State* connectedState;
        

    public:
        MQTT():ActiveObject("MQTT", 9216, 5)
        {
            instance = this;
            createStateMachine();
        }

        static MQTT* getInstance()
        {
            return instance;
        }

        void initParams();
        void mainLoop();
        void createStateMachine();

        AWS_IoT_Client client;
        char cPayload[100];

        IoT_Publish_Message_Params paramsQOS0;
        IoT_Client_Init_Params mqttInitParams;
        IoT_Client_Connect_Params connectParams;

        uint8_t sendMQTTmsg(cJSON *s);
        void wifiConnected();
        void wifiDisconnected();

        friend class MQTT_FSM_State;
        friend class MQTT_Init_State;
        friend class MQTT_Connecting_State;
        friend class MQTT_Connected_State;

};



#include "cJSON.h"
#include "aws_iot_mqtt_client_interface.h"
#include "ActiveObject.h"

class MQTT: public ActiveObject
{
    private:
        static MQTT* instance;
        

    public:
        MQTT():ActiveObject("MQTT", 9216, 5)
        {
            initParams();
            instance = this;
        }

        static MQTT* getInstance()
        {
            return instance;
        }

        void initParams();
        void mainLoop();

        AWS_IoT_Client client;
        char cPayload[100];

        bool wifi_operational;
        IoT_Publish_Message_Params paramsQOS0;
        IoT_Client_Init_Params mqttInitParams;
        IoT_Client_Connect_Params connectParams;

        uint8_t sendMQTTmsg(cJSON *s);
        void wifiConnected();
        void wifiDisconnected();

};

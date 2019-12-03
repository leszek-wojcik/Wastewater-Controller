#include "ActiveObject.h"

class MQTT: public ActiveObject
{
    private:

    public:
        MQTT():ActiveObject("MQTT", 9216, 5)
        {
            xmitQueue = xQueueCreate(20, sizeof(MRequest *));
            initParams();
        }

        void initParams();
        void mainLoop();

        QueueHandle_t xmitQueue = NULL;
        AWS_IoT_Client client;
        char cPayload[100];

        bool wifi_operational;
        IoT_Publish_Message_Params paramsQOS0;
        IoT_Client_Init_Params mqttInitParams;
        IoT_Client_Connect_Params connectParams;
};

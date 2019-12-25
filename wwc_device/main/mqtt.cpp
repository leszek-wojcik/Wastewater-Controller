#include <stdio.h>
#include <string.h>

#include "esp_log.h"

#include "ActiveObject.h"
#include "MethodRequest.h"
#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"
#include "mqttfsm.h"

#include "cJSON.h"
#include "aws_iot_mqtt_client_interface.h"


static const char *TAG = "MQTT";

MQTT* MQTT::instance = NULL;
MQTT_FSM_State* MQTT::currentState = NULL;
MQTT_Init_State* MQTT::initState = NULL;
MQTT_Connecting_State* MQTT::connectingState = NULL;
MQTT_Connected_State* MQTT::connectedState = NULL;

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");


char HostAddress[255] = CONFIG_MQTT_HOST;
uint32_t port = CONFIG_MQTT_PORT;

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) 
{
    ESP_LOGW(TAG, "MQTT Disconnect");
    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) { return; }

    if(aws_iot_is_autoreconnect_enabled(pClient)) 
    {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } 
    else 
    {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) 
        {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } 
        else 
        {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void MQTT::initParams()
{
    mqttInitParams = iotClientInitParamsDefault;
    connectParams = iotClientConnectParamsDefault;

    mqttInitParams.enableAutoReconnect = false; // We enable this later below
    mqttInitParams.pHostURL = HostAddress;
    mqttInitParams.port = port;

    mqttInitParams.pRootCALocation = (const char *)aws_root_ca_pem_start;
    mqttInitParams.pDeviceCertLocation = (const char *)certificate_pem_crt_start;
    mqttInitParams.pDevicePrivateKeyLocation = (const char *)private_pem_key_start;

    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnectCallbackHandler;
    mqttInitParams.disconnectHandlerData = NULL;

    connectParams.keepAliveIntervalInSec = 5;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;
    connectParams.pClientID = CONFIG_AWS_CLIENT_ID;
    connectParams.clientIDLen = (uint16_t) strlen(CONFIG_AWS_CLIENT_ID);
    connectParams.isWillMsgPresent = false;

    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = (void *) cPayload;
    paramsQOS0.isRetained = 0;
}

void MQTT::createStateMachine()
{
    initState        = new MQTT_Init_State(this);
    connectingState  = new MQTT_Connecting_State(this);
    connectedState   = new MQTT_Connected_State(this);

    currentState = initState;
    currentState->onEntry();
}

void MQTT::throttle(uint32_t tmo)
{
    IoT_Error_t rc = SUCCESS;
    rc = aws_iot_mqtt_yield(&client, tmo);

    if (rc !=SUCCESS)
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "error form mqqt yield");
    }
}

uint8_t MQTT::sendMQTTmsg(cJSON *s)
{

    auto f = [=] () 
        {
            if (currentState != connectedState)
            {
                printf("MQTT not operational dropping mqtt msg\n");
                cJSON_Delete(s);
            }
            else
            {
                sprintf(cPayload,"%s", cJSON_Print(s));
                paramsQOS0.payloadLen = strlen(cPayload);
//                printf("json at aws context:\n%s",cJSON_Print(s));
                aws_iot_mqtt_publish(&client, topic, topicLen, &paramsQOS0);
                cJSON_Delete(s);
            }
        };

    auto mr = new MRequest(NULL, f);
    return xQueueSend(mrQueue, &mr, 0);
}

void MQTT::wifiConnected()
{
    auto mr = new MRequest(NULL, [=](){currentState->wifiConnected();});
    xQueueSend(mrQueue, &mr, 0);
}

void MQTT::wifiDisconnected()
{
    auto mr = new MRequest(NULL,[=](){currentState->wifiDisconnected();} );
    xQueueSend(mrQueue, &mr, 0);
}

void MQTT::established()
{
    auto mr = new MRequest(NULL,[=](){currentState->established();} );
    xQueueSend(mrQueue, &mr, 0);
}

void MQTT::startThrottleTmr()
{
    throttleTmr =  createTimer (
                [=] () { throttle(); },
               1000/portTICK_PERIOD_MS  );
}

void MQTT::stopThrottleTmr()
{
    stopTimer (throttleTmr);
}

void MQTT::ping()
{
    sprintf(cPayload,"%s", "ping");
    paramsQOS0.payloadLen = strlen(cPayload);
    aws_iot_mqtt_publish(&client, pingTopic, pingTopicLen, &paramsQOS0);
    numberOfPings++;

    if (numberOfPings > 3)
        currentState->onError();
}

void MQTT::startPingTmr()
{
    pingTmr =  createTimer (
                [=] () { ping(); },
               10000/portTICK_PERIOD_MS  );
}

void MQTT::stopPingTmr()
{
    stopTimer (pingTmr);
}


void MQTT::activity()
{
    if( activityInd == false )
    {
        abort();
    }
    activityInd = false;
}

void MQTT::startActivityTmr()
{
    activityTmr =  createTimer (
                [=] () { activity(); },
               3600000/portTICK_PERIOD_MS  );
}
void MQTT::stopActivityTmr()
{
    stopTimer(activityTmr);
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, 
                                    char *topicName, 
                                    uint16_t topicNameLen, 
                                    IoT_Publish_Message_Params *params, 
                                    void *pData) 
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Received from cloud");
    ESP_LOGI(__PRETTY_FUNCTION__, "%.*s\t%.*s", topicNameLen, 
                                topicName, 
                                (int) params->payloadLen, 
                                (char *)params->payload);

    MQTT::getInstance()->activityPresent();

}

void MQTT::activityPresent()
{
    activityInd = true;
}

void ping_callback_handler(AWS_IoT_Client *pClient, 
                                    char *topicName, 
                                    uint16_t topicNameLen, 
                                    IoT_Publish_Message_Params *params, 
                                    void *pData) 
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Received from cloud");
    ESP_LOGI(__PRETTY_FUNCTION__, "%.*s\t%.*s", topicNameLen, 
                                topicName, 
                                (int) params->payloadLen, 
                                (char *)params->payload);

    MQTT::getInstance()->established();
}

void MQTT::connect()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI("MQTT", "Connecting ...");
    rc = aws_iot_mqtt_connect(&client, &connectParams);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error(%d) connecting to %s:%d", 
                rc, 
                mqttInitParams.pHostURL, 
                mqttInitParams.port);
    }

    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }

}

void MQTT::subscribePing()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI(__PRETTY_FUNCTION__, "Subscribing to %s...", pingTopic);
    rc = aws_iot_mqtt_subscribe(&client, 
                                pingTopic, 
                                pingTopicLen, 
                                QOS0, 
                                ping_callback_handler, 
                                NULL);

    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error subscribing : %d ", rc);
        abort();
    }
}

void MQTT::unsubscribePing()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI(__PRETTY_FUNCTION__, "Unsubscribing %s...", pingTopic);

    rc = aws_iot_mqtt_unsubscribe(&client, pingTopic, pingTopicLen);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error unsubscribing : %d ", rc);
        abort();
    }

}

void MQTT::subscribeTopic()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI(__PRETTY_FUNCTION__, "Subscribing to %s...", topic);
    rc = aws_iot_mqtt_subscribe(&client, 
                                topic, 
                                topicLen, 
                                QOS0, 
                                iot_subscribe_callback_handler, 
                                NULL);


    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error subscribing : %d ", rc);
        abort();
    }
}

void MQTT::unsubscribeTopic()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI(__PRETTY_FUNCTION__, "Unsubscribing %s...", pingTopic);

    rc = aws_iot_mqtt_unsubscribe(&client, topic, topicLen);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error unsubscribing : %d ", rc);
        abort();
    }

}

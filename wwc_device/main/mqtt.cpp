#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include <string>
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "ActiveObject.h"
#include "MethodRequest.h"
#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"


static const char *TAG = "MQTT";
const char *TOPIC = "wwc";
const int TOPIC_LEN = strlen(TOPIC);

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


char HostAddress[255] = AWS_IOT_MQTT_HOST;
uint32_t port = AWS_IOT_MQTT_PORT;

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, 
                                    char *topicName, 
                                    uint16_t topicNameLen, 
                                    IoT_Publish_Message_Params *params, 
                                    void *pData) 
{
    ESP_LOGI(TAG, "Received from cloud");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, 
                                topicName, 
                                (int) params->payloadLen, 
                                (char *)params->payload);

}

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

    connectParams.keepAliveIntervalInSec = 10;
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

void MQTT::mainLoop()
{

    MRequest * msg;
    IoT_Error_t rc = SUCCESS;

    do 
    {
        xQueueReceive( mrQueue, &msg, portMAX_DELAY );
        (*(msg->func))();
        delete msg;
    }
    while( currentState != connectingState);

    currentState->stateTransition(connectedState);

    while(
        (NETWORK_ATTEMPTING_RECONNECT == rc || 
         NETWORK_RECONNECTED == rc || 
         SUCCESS == rc)) 
    {

        //Max time the yield function will wait for read messages
        rc = aws_iot_mqtt_yield(&client, 100);
        if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            // If the client is attempting to reconnect we will skip the rest of the loop.
            continue;
        }

        if(xQueueReceive( mrQueue, &msg, 1000 ))
        {
            (*(msg->func))();
            delete msg;

        }
    }

    ESP_LOGE(TAG, "An error occurred in the main loop.");
    abort();
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
                aws_iot_mqtt_publish(&client, TOPIC, TOPIC_LEN, &paramsQOS0);
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

void MQTT_FSM_State::stateTransition(MQTT_FSM_State *next)
{
    onExit();
    next->onEntry();
    context->currentState = next;
}

void MQTT_Init_State::wifiConnected()
{
    stateTransition(context->connectingState);
}

void MQTT_Connecting_State::wifiConnected()
{
    ESP_LOGE(TAG, "wifi connected while in connecting state.");
    abort();
}

void MQTT_Connected_State::wifiConnected()
{
    ESP_LOGE(TAG, "wifi connected while in connected state.");
    abort();
}

void MQTT_Init_State::wifiDisconnected()
{
    ESP_LOGE(TAG, "wifi disconnected while in init state.");
    abort();
}

void MQTT_Connecting_State::wifiDisconnected()
{
    ESP_LOGE(TAG, "wifi disconnected while in connecting state.");
    //TODO::perform cleanup
}

void MQTT_Connected_State::wifiDisconnected()
{
    ESP_LOGE(TAG, "wifi disconnected while in connected state.");
    //TODO::perform cleanup
}

void MQTT_Init_State::established()
{
    ESP_LOGE(TAG, "established while in init state.");
    abort();
}

void MQTT_Connecting_State::established()
{
    //TODO::implement cleanup
}

void MQTT_Connected_State::established()
{
    ESP_LOGE(TAG, "established while in established state.");
    abort();
}

void MQTT_Init_State::onEntry()
{
    ESP_LOGI(TAG, "Init state");
    IoT_Error_t rc = FAILURE;
    context->initParams();

    rc = aws_iot_mqtt_init(&context->client, &context->mqttInitParams);

    if(SUCCESS != rc) 
    {
        ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

}

void MQTT_Connecting_State::onEntry()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI(TAG, "Connecting ...");
    do 
    {
        rc = aws_iot_mqtt_connect(&context->client, &context->connectParams);
        if(SUCCESS != rc) 
        {
            ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", 
                    rc, 
                    context->mqttInitParams.pHostURL, 
                    context->mqttInitParams.port);

            vTaskDelay(10000 / portTICK_RATE_MS);
        }
    } 
    while(SUCCESS != rc);

    rc = aws_iot_mqtt_autoreconnect_set_status(&context->client, true);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }


    ESP_LOGI(TAG, "Subscribing to %s...", TOPIC);
    rc = aws_iot_mqtt_subscribe(&context->client, 
                                TOPIC, 
                                TOPIC_LEN, 
                                QOS0, 
                                iot_subscribe_callback_handler, 
                                NULL);

    if(SUCCESS != rc) 
    {
        ESP_LOGE(TAG, "Error subscribing : %d ", rc);
        abort();
    }
    ESP_LOGI(TAG, "Subscribed to %s...", TOPIC);
}

void MQTT_Connected_State::onEntry()
{
    ESP_LOGI(TAG, "Connected ...");
}

void MQTT_Init_State::onExit()
{
    ESP_LOGI(TAG, "exit init state ...");
}

void MQTT_Connecting_State::onExit()
{
    ESP_LOGI(TAG, "exit connecting state ...");
}

void MQTT_Connected_State::onExit()
{
    ESP_LOGI(TAG, "exit connected state ...");
}

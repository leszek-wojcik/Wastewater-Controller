#include "esp_log.h"
#include "aws_iot_mqtt_client_interface.h"
#include "mqtt.h"
#include "mqttfsm.h"

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
    //this is tweek until we get connecting state right
    stateTransition(context->connectedState);
}

void MQTT_Connecting_State::wifiConnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi connected while in connecting state.");
    abort();
}

void MQTT_Connected_State::wifiConnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi connected while in connected state.");
    abort();
}

void MQTT_Init_State::wifiDisconnected()
{

}

void MQTT_Connecting_State::wifiDisconnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi disconnected while in connecting state.");
    //TODO::perform cleanup
}

void MQTT_Connected_State::wifiDisconnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi disconnected while in connected state.");
    //TODO::perform cleanup
}

void MQTT_Init_State::established()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "established while in init state.");
    abort();
}

void MQTT_Connecting_State::established()
{
    //TODO::implement cleanup
}

void MQTT_Connected_State::established()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "established while in established state.");
    abort();
}

void MQTT_Init_State::onEntry()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Init state");
    IoT_Error_t rc = FAILURE;
    context->initParams();

    rc = aws_iot_mqtt_init(&context->client, &context->mqttInitParams);

    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

}

void MQTT_Connecting_State::onEntry()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI(__PRETTY_FUNCTION__, "Connecting ...");
    do 
    {
        rc = aws_iot_mqtt_connect(&context->client, &context->connectParams);
        if(SUCCESS != rc) 
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "Error(%d) connecting to %s:%d", 
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
        ESP_LOGE(__PRETTY_FUNCTION__, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }


    ESP_LOGI(__PRETTY_FUNCTION__, "Subscribing to %s...", context->topic);
    rc = aws_iot_mqtt_subscribe(&context->client, 
                                context->topic, 
                                context->topicLen, 
                                QOS0, 
                                iot_subscribe_callback_handler, 
                                NULL);

    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error subscribing : %d ", rc);
        abort();
    }
    ESP_LOGI(__PRETTY_FUNCTION__, "Subscribed to %s...", context->topic);
}

void MQTT_Connected_State::onEntry()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Connected ...");
    context->startThrottleTmr();
}

void MQTT_Init_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit init state ...");
}

void MQTT_Connecting_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit connecting state ...");
}

void MQTT_Connected_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit connected state ...");
    context->stopThrottleTmr();
}

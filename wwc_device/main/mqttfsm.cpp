#include "esp_log.h"
#include "aws_iot_mqtt_client_interface.h"
#include "mqtt.h"
#include "mqttfsm.h"

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

void MQTT_Init_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit init state ...");
}

void MQTT_Init_State::wifiDisconnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "abnormal scenario in Init state");
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

void MQTT_Init_State::onError()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Error in init state ...");
    abort();
}

void MQTT_Init_State::established()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "established while in init state.");
    abort();
}



void MQTT_Connecting_State::wifiConnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi connected while in connecting state.");
    abort();
}


void MQTT_Connecting_State::wifiDisconnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi disconnected while in connecting state.");
    stateTransition(context->initState);
}


void MQTT_Connecting_State::established()
{
    context->stopPingTmr();
    context->stopThrottleTmr();
    stateTransition(context->connectedState);
}

void MQTT_Connecting_State::onEntry()
{
    context->connect();
    context->subscribePing();
    context->numberOfPings = 0;
    context->ping();
    context->startPingTmr();
    context->throttle(1000);
    context->startThrottleTmr();
}

void MQTT_Connecting_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit connecting state ...");

    context->stopPingTmr();
    context->stopThrottleTmr();
    context->unsubscribePing();
}

void MQTT_Connecting_State::onError()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Error in connecting state ...");
    stateTransition(context->connectingState);
}

void MQTT_Connected_State::established()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "established while in established state.");
    abort();
}

void MQTT_Connected_State::wifiDisconnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi disconnected while in connected state.");
    stateTransition(context->initState);
}


void MQTT_Connected_State::onEntry()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Connected ...");
    context->subscribeTopic();
    context->startThrottleTmr();
    context->throttle(1000);
}

void MQTT_Connected_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit connected state ...");
    context->unsubscribeTopic();
    context->stopThrottleTmr();
}

void MQTT_Connected_State::onError()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Error in connected state ...");
    abort();
}

void MQTT_Connected_State::wifiConnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi connected while in connected state.");
    abort();
}

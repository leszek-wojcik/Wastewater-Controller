#include "esp_log.h"
#include "aws_iot_mqtt_client_interface.h"
#include "mqtt.h"
#include "mqttfsm.h"

////////////////////////
void MQTT_FSM_State::stateTransition(MQTT_FSM_State *next)
{
    onExit();
    next->onEntry();
    context->currentState = next;
}

////////////////////////
void MQTT_Init_State::onEntry()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Init state Entry");
    context->init();
    context->startInitTmr();
}

void MQTT_Init_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit init state ...");
    context->stopInitTmr();
}

void MQTT_Init_State::onError()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Error in init state ...");
    stateTransition(context->initState);
}

void MQTT_Init_State::wifiConnected()
{
    stateTransition(context->connectingState);
}

void MQTT_Init_State::wifiDisconnected()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "wifi disconnected in Init state");
}

void MQTT_Init_State::timeReceived(MqttMessage_t msg)
{
    ESP_LOGE("MQTT", "timeReceived while in init state.");
    abort();
}

void MQTT_Init_State::subscribeTopic(MqttMessage_t s, MqttTopicCallback_t f)
{
    ESP_LOGI("MQTT", "Requested subscribtion: %s", s->c_str());
    context->addToSubscriptions(s,f);
}

void MQTT_Init_State::subjectSend(MqttTopic_t topic, MqttMessage_t message)
{
    ESP_LOGW("MQTT", "discarding msg for topic %s", topic->c_str());
}

void MQTT_Init_State::unsubscribeTopic(MqttTopic_t t)
{
    ESP_LOGI("MQTT", "removed from subscribtions: %s", t->c_str());
    context->removeFromSubscriptions(t);
}

////////////////////////
void MQTT_Connecting_State::onEntry()
{
    
    IoT_Error_t rc = FAILURE;
    
    // Firstly cleanup for any previous session
    if ( context->mqttInitialized)
    {
        rc = aws_iot_mqtt_free(&context->client);
        if(SUCCESS != rc) 
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "aws_iot_mqtt_free returned error : %d ", rc);
            abort();
        }
    }
    
    // Initialize of reinitialize parameters for mqtt
    context->initParams();

    // Initialize MQTT
    rc = FAILURE;
    rc = aws_iot_mqtt_init(&context->client, &context->mqttInitParams);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }
    
    context->mqttInitialized = true;

    context->startSafeGuardTmr();
    if (context->connectMQTT())
    {
        context->executeSubscribeTime();
        context->startThrottleTmr();
    }
    else 
    {
        context->startReconnectTmr();
    }
}

void MQTT_Connecting_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit connecting state ...");

    context->stopThrottleTmr();
    context->stopReconnectTmr();
    context->stopSafeGuardTmr();
}

void MQTT_Connecting_State::onError()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Error in connecting state ...");
    stateTransition(context->initState);
}
void MQTT_Connecting_State::wifiConnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi connected while in connecting state.");
    abort();
}


void MQTT_Connecting_State::wifiDisconnected()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "wifi disconnected while in connecting state.");
    stateTransition(context->initState);
}


void MQTT_Connecting_State::timeReceived(MqttMessage_t msg)
{
    ESP_LOGI(__PRETTY_FUNCTION__, "time received while in connecting state.");
    context->processTimeMessage(msg);
    context->executeUnsubscribeTime();
    stateTransition(context->connectedState);
}


void MQTT_Connecting_State::subscribeTopic(MqttTopic_t s, MqttTopicCallback_t f)
{
    ESP_LOGI("MQTT", "Requested subscribtion: %s", s->c_str());
    context->addToSubscriptions(s,f);
}

void MQTT_Connecting_State::subjectSend(MqttTopic_t topic, MqttMessage_t message)
{
    ESP_LOGW("MQTT", "discarding msg for topic %s", topic->c_str());
}

void MQTT_Connecting_State::unsubscribeTopic(MqttTopic_t t)
{
    ESP_LOGI("MQTT", "removed from subscribtions: %s", t->c_str());
    context->removeFromSubscriptions(t);
    context->executeUnsubscribeTopic(t);
}

////////////////////////
void MQTT_Connected_State::onEntry()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Connected ...");
    context->runStateCallback(true,true);
    context->subscribeTopics();
    context->startThrottleTmr();
    context->startActivityTmr();
    context->startObtainTimeTmr();
    context->requestShadow();
    context->throttle(1000);
}

void MQTT_Connected_State::onExit()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "exit connected state ...");
    context->runStateCallback(false,false);
    context->unsubscribeTopics();
    context->executeDisconnect();
    context->stopThrottleTmr();
    context->stopActivityTmr();
    context->stopObtainTimeTmr();
}

void MQTT_Connected_State::onError()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Error in connected state ...");
    stateTransition(context->connectingState);
}

void MQTT_Connected_State::timeReceived(MqttMessage_t msg)
{
    context->processTimeMessage(msg);
    context->executeUnsubscribeTime();
    context->runStateCallback(true,true);
}

void MQTT_Connected_State::wifiDisconnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi disconnected while in connected state.");
    stateTransition(context->initState);
}

void MQTT_Connected_State::wifiConnected()
{
    ESP_LOGE(__PRETTY_FUNCTION__, "wifi connected while in connected state.");
    abort();
}

void MQTT_Connected_State::subscribeTopic(MqttTopic_t s, MqttTopicCallback_t f)
{
    ESP_LOGI("MQTT", "Requested subscribtion: %s", s->c_str());
    context->addToSubscriptions(s,f);
    context->executeSubscribeTopic(s);
}

void MQTT_Connected_State::subjectSend(MqttTopic_t t, MqttMessage_t m )
{
    ESP_LOGI("MQTT","sending:%s:%s", t->c_str(), m->c_str() );
    context->executeSubjectSend(t,m);
}

void MQTT_Connected_State::unsubscribeTopic(MqttTopic_t t)
{
    ESP_LOGI("MQTT", "removed from subscribtions: %s", t->c_str());
    context->removeFromSubscriptions(t);
    context->executeUnsubscribeTopic(t);
}

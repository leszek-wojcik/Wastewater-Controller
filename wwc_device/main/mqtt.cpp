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
    ESP_LOGW(TAG, "MQTT Disconnect from AWS IOT framework");
    MQTT::getInstance()->onError();
}

MQTT::MQTT(WiFi *wifi):ActiveObject("MQTT", 9216, 5),wifi(wifi)
{
    instance = this;

    throttleTmr  = NULL;
    pingTmr      = NULL;
    activityTmr  = NULL;
    initTmr      = NULL;
    reconnectTmr = NULL;
    safeGuardTmr = NULL;

    strcpy(topic,"wwc/");
    strcat(topic,CONFIG_AWS_CLIENT_ID);
    strcat(topic,"/control");
    topicLen = strlen(topic);

    strcpy(pingTopic,"wwc/");
    strcat(pingTopic,CONFIG_AWS_CLIENT_ID);
    strcat(pingTopic,"/ping");
    pingTopicLen = strlen(pingTopic);
    activityInd = false;

    createStateMachine();
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
        onError();
    }
}

void MQTT::subjectSend(MqttTopic_t sub, MqttMessage_t msg)
{
    executeMethod( [=](){ currentState->subjectSend( sub, msg );} );
}

void MQTT::sendMQTTmsg(MqttTopic_t topic, MqttMessage_t msg )
{
    sprintf(cPayload,"%s",msg->c_str());
    paramsQOS0.payloadLen = strlen(cPayload);
    aws_iot_mqtt_publish(&client, topic->c_str(), topic->length(), &paramsQOS0);
}

uint8_t MQTT::answerPing()
{
    printf("answer ping not yet implemented\n");
    return 0;
}

void MQTT::wifiConnected()
{
    executeMethod( [=](){currentState->wifiConnected();} );
}

void MQTT::wifiDisconnected()
{
    executeMethod( [=](){currentState->wifiDisconnected();});
}

void MQTT::pingReceived()
{
    executeMethod( [=](){currentState->pingReceived();});
}

void MQTT::onError()
{
    executeMethod( [=](){currentState->onError();});
}


void MQTT::startThrottleTmr()
{
    ESP_LOGI("MQTT", "start throttle timer");
    createTimer (
            &throttleTmr, 
            [=] () { throttle(); },
            1000/portTICK_PERIOD_MS  );
}

void MQTT::stopThrottleTmr()
{
    ESP_LOGI("MQTT", "stop throttle timer");
    stopTimer (&throttleTmr);
}

void MQTT::ping()
{
    ESP_LOGI("MQTT", "ping ...");
    sprintf(cPayload,"%s", "ping");
    paramsQOS0.payloadLen = strlen(cPayload);
    aws_iot_mqtt_publish(&client, pingTopic, pingTopicLen, &paramsQOS0);
}

void MQTT::startPingTmr()
{
    ESP_LOGI("MQTT", "start ping timer");
    createTimer (
            &pingTmr ,  
            [=] () { ping(); },
            10000/portTICK_PERIOD_MS  );
}

void MQTT::stopPingTmr()
{
    ESP_LOGI("MQTT", "stop ping timer");
    stopTimer (&pingTmr);
}

void MQTT::safeGuard()
{
    ESP_LOGW("MQTT", "safeGuard ...");
    currentState->onError();
}

void MQTT::startSafeGuardTmr()
{
    ESP_LOGI("MQTT", "start safeguard timer");
    createTimer (
            &safeGuardTmr, 
            [=] () { safeGuard(); },
            32000/portTICK_PERIOD_MS  );
}

void MQTT::stopSafeGuardTmr()
{
    ESP_LOGI("MQTT", "stop safeguard timer");
    stopTimer( &safeGuardTmr );
}

void MQTT::reconnectMQTT()
{
    ESP_LOGI("MQTT", "reconnecting ...");

    if (currentState != connectingState )
    {
        abort();
    }
    //redo entry acction with attempt to connect
    currentState->onEntry();
    
}

void MQTT::startReconnectTmr()
{
    ESP_LOGI("MQTT", "start reconnect timer");
    createOneTimeTimer (
            &reconnectTmr,
            [=] () { reconnectMQTT(); },
            15000/portTICK_PERIOD_MS  );
}

void MQTT::stopReconnectTmr()
{
    ESP_LOGI("MQTT", "stop reconnect timer");
    stopTimer (&reconnectTmr);
}

void MQTT::activity()
{
    if( activityInd == false )
    {
        ESP_LOGW("MQTT", "no activity ...");
        currentState->onError();
    }
    activityInd = false;
}

void MQTT::startActivityTmr()
{
    ESP_LOGI("MQTT", "start activity timer");
    createTimer (
            &activityTmr,
            [=] () { activity(); },
            600000/portTICK_PERIOD_MS  );
}
void MQTT::stopActivityTmr()
{
    ESP_LOGI("MQTT", "stop activity timer");
    stopTimer(&activityTmr);
}


void MQTT::init()
{
    ESP_LOGI(__PRETTY_FUNCTION__, "init");
    IoT_Error_t rc = FAILURE;
    initParams();
    rc = aws_iot_mqtt_init(&client, &mqttInitParams);

    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }
    disconnectWiFi();
    vTaskDelay(1000/portTICK_PERIOD_MS);

    executeMethod( [=](){connectWiFi(); toggleWiFi(); });
}

void MQTT::startInitTmr()
{
    ESP_LOGI("MQTT", "start init timer");
    createTimer (
            &initTmr ,
            [=] () {init();},
            15000/portTICK_PERIOD_MS);

}
void MQTT::stopInitTmr()
{

    ESP_LOGI("MQTT", "stop init timer");
    stopTimer(&initTmr);
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, 
                                    char *topicName, 
                                    uint16_t topicNameLen, 
                                    IoT_Publish_Message_Params *params, 
                                    void *pData) 
{
    ESP_LOGI("MQTT", " Received from could %.*s\t%.*s", topicNameLen, 
                                topicName, 
                                (int) params->payloadLen, 
                                (char *)params->payload);


    MqttTopic_t topic (new string( topicName, topicNameLen ));
    MqttMessage_t msg (new string( (char*)params->payload, params->payloadLen ));

    MQTT::getInstance()->dispatchMessage(topic,msg);

}

void MQTT::dispatchMessage(MqttTopic_t topic, MqttMessage_t msg)
{
    executeMethod( [=](){this->processIncommingMessage(topic,msg); }  );
}


void MQTT::processIncommingMessage(MqttTopic_t topic, MqttMessage_t msg  )
{
    activityInd = true;
    for (auto a : subscriptions)
    {
        if ( *a.first == *topic)
        {
            a.second(msg);
            break;
        }
    }
}

void ping_callback_handler(AWS_IoT_Client *pClient, 
                                    char *topicName, 
                                    uint16_t topicNameLen, 
                                    IoT_Publish_Message_Params *params, 
                                    void *pData) 
{
    ESP_LOGI("MQTT", "...Received ping from cloud");
    MQTT::getInstance()->pingReceived();
}

bool MQTT::connectMQTT()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI("MQTT", "Connecting ...");
    rc = aws_iot_mqtt_connect(&client, &connectParams);
    if(SUCCESS != rc) 
    {
        ESP_LOGW(__PRETTY_FUNCTION__, "Error(%d) connecting to %s:%d", 
                rc, 
                mqttInitParams.pHostURL, 
                mqttInitParams.port);
        return false;
    }
    return true;

}

void MQTT::connectWiFi()
{
    wifi->connect();
}

void MQTT::disconnectWiFi()
{
    wifi->disconnect();
}

void MQTT::toggleWiFi()
{
    wifi->toggle();
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
    }
}

void MQTT::unsubscribePing()
{
    IoT_Error_t rc = FAILURE;
    rc = aws_iot_mqtt_unsubscribe(&client, pingTopic, pingTopicLen);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error unsubscribing : %d ", rc);
    }

}

void MQTT::subscribeTopics()
{
    for ( auto s: subscriptions)
    {
        executeSubscribeTopic ( s.first );
    }
}

void MQTT::executeSubscribeTopic(MqttTopic_t topic)
{

    IoT_Error_t rc = FAILURE;
    ESP_LOGI("MQTT", "Subscribing to %s...", topic->c_str());
    rc = aws_iot_mqtt_subscribe(&client, 
            topic->c_str(), 
            topic->length(), 
            QOS0, 
            iot_subscribe_callback_handler, 
                                    NULL);

    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error subscribing : %d ", rc);
        onError();
    }

}

void MQTT::unsubscribeTopics()
{
    for ( auto s: subscriptions)
    {
        executeUnsubscribeTopic(s.first);
    }

}

void MQTT::unsubscribeTopic(MqttTopic_t topic)
{
    executeMethod( [=] (){currentState->unsubscribeTopic(topic);} );
}

void MQTT::executeUnsubscribeTopic(MqttTopic_t topic)
{
    IoT_Error_t rc = FAILURE;

    ESP_LOGI("MQTT", "Unsubscribing %s...", topic->c_str());
    rc = aws_iot_mqtt_unsubscribe(&client, topic->c_str(), topic->length());
    if(SUCCESS != rc) 
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error unsubscribing : %d ", rc);
    }
}

bool MQTT::isConnected()
{
    return (currentState == connectedState);
}

void MQTT::subscribeTopic(MqttTopic_t s, MqttTopicCallback_t f)
{
    executeMethod( [=](){  currentState->subscribeTopic(s,f); });
}

void MQTT::addToSubscriptions(MqttTopic_t s, MqttTopicCallback_t f)
{
    subscriptions[s] = f;
}

void MQTT::removeFromSubscriptions( MqttTopic_t t)
{
    subscriptions.erase(t);
}

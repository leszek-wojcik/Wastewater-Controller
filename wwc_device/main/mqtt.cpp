#include <stdio.h>

#include "esp_log.h"


#include "ActiveObject.h"
#include "MethodRequest.h"
#include "wwc.h"
#include "wifi.h"
#include "mqtt.h"
#include "mqttfsm.h"
#include "topics.h"

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

MQTT::MQTT(WiFi *wifi):ActiveObject("MQTT", 9216, 5),wifi(wifi)
{
    instance = this;

    throttleTmr         = NULL;
    obtainTimeTmr       = NULL;
    activityTmr         = NULL;
    initTmr             = NULL;
    reconnectTmr        = NULL;
    safeGuardTmr        = NULL;

    mqttTimeTopic.reset(new string(TIMESTAMP_TOPIC));

    activityInd = false;

    createStateMachine();
    stateCallback       = NULL;
}

void MQTT::registerStateCallback(MqttStateCallback_t callback)
{
    stateCallback = callback;
}

void MQTT::runStateCallback(bool connected, bool timeUpdated)
{
    if (stateCallback !=NULL)
    {
        stateCallback(connected, timeUpdated);
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
        onError();
    }
}

void MQTT::subjectSend(MqttTopic_t sub, MqttMessage_t msg)
{
    executeMethod( [=](){ currentState->subjectSend( sub, msg );} );
}

void MQTT::executeSubjectSend(MqttTopic_t topic, MqttMessage_t msg )
{
    IoT_Error_t rc = SUCCESS;
    activityInd = true;
    sprintf(cPayload,"%s",msg->c_str());
    paramsQOS0.payloadLen = strlen(cPayload);
    rc = aws_iot_mqtt_publish(&client, topic->c_str(), topic->length(), &paramsQOS0);
    if (rc !=SUCCESS)
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "error form mqqt yield");
        onError();
    }
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

void MQTT::timeReceived(MqttMessage_t msg)
{
    executeMethod( [=](){currentState->timeReceived(msg);});
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
            1000);
}

void MQTT::stopThrottleTmr()
{
    ESP_LOGI("MQTT", "stop throttle timer");
    stopTimer (&throttleTmr);
}


void MQTT::startObtainTimeTmr()
{
    ESP_LOGI("MQTT", "start Obtain time timer");
    createTimer (
            &obtainTimeTmr ,  
            [=] () { obtainTime(); },
            10800000); //3hr
}

void MQTT::stopObtainTimeTmr()
{
    ESP_LOGI("MQTT", "stop ping timer");
    stopTimer (&obtainTimeTmr);
}

void MQTT::obtainTime()
{
    ESP_LOGI("MQTT", "Atttempt To obtain time...");
    executeSubscribeTime();
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
            32000);
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
            15000);
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
            1000000);
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
            15000);

}
void MQTT::stopInitTmr()
{

    ESP_LOGI("MQTT", "stop init timer");
    stopTimer(&initTmr);
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

void MQTT::executeDisconnect()
{
    IoT_Error_t rc = FAILURE;
    ESP_LOGI("MQTT", "Disconnecting ...");
    rc = aws_iot_mqtt_disconnect(&client);
    if(SUCCESS != rc) 
    {
        ESP_LOGW(__PRETTY_FUNCTION__, "Error(%d) connecting to %s:%d", 
                rc, 
                mqttInitParams.pHostURL, 
                mqttInitParams.port);
    }
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

void MQTT::executeSubscribeTime()
{
    // We are calling this function within MQTT AO context
    addToSubscriptions(mqttTimeTopic, [=](MqttMessage_t a) { this->timeReceived(a); } );
    executeSubscribeTopic ( mqttTimeTopic ); 
}

void MQTT::executeUnsubscribeTime()
{
    executeUnsubscribeTopic( mqttTimeTopic );
    removeFromSubscriptions( mqttTimeTopic );
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

void MQTT::processTimeMessage( MqttMessage_t msg)
{
    system_clock::time_point tp;
    cJSON *item;
    cJSON *json = cJSON_Parse(msg->c_str());
    item = cJSON_GetObjectItem(json,"timestamp");
    long es;

    if ( item != NULL )
    {
        if ( cJSON_IsNumber(item) )
        {
            //convert ms to seconds
            es = (long) (item->valuedouble / 1000);

            struct timeval tv = { es, 0};
            settimeofday (&tv, NULL);
            tp = system_clock::now();
            time_t t = system_clock::to_time_t(tp);

            ESP_LOGI("MQTT", "%s",ctime(&t));
        }
    }
    cJSON_Delete(json);
}

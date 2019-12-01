/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file subscribe_publish_sample.c
 * @brief simple MQTT publish and subscribe on the same topic
 *
 * This example takes the parameters from the build configuration and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to the same topic - "test_topic/esp32"
 *
 * Some setup is required. See example README for details.
 *
 */
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


static const char *TAG = "subpub";

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


QueueHandle_t xmitQueue = NULL;
AWS_IoT_Client client;
char cPayload[100];
IoT_Publish_Message_Params paramsQOS0;
const char *TOPIC = "wwc";
const int TOPIC_LEN = strlen(TOPIC);
bool wifi_operational = false;

void aws_iot_task(void *param) {

    MRequest * msg;
    xmitQueue = xQueueCreate(20, sizeof(MRequest *));
    IoT_Error_t rc = FAILURE;

    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;


    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", 
                    VERSION_MAJOR, 
                    VERSION_MINOR, 
                    VERSION_PATCH, 
                    VERSION_TAG);

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

    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

    do 
    {
        xQueueReceive( xmitQueue, &msg, portMAX_DELAY );
        (*(msg->func))();
        delete msg;
    }while(  wifi_operational == false);

    connectParams.keepAliveIntervalInSec = 10;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;
    connectParams.pClientID = CONFIG_AWS_CLIENT_ID;
    connectParams.clientIDLen = (uint16_t) strlen(CONFIG_AWS_CLIENT_ID);
    connectParams.isWillMsgPresent = false;

    ESP_LOGI(TAG, "Connecting to AWS...");
    do 
    {
        rc = aws_iot_mqtt_connect(&client, &connectParams);
        if(SUCCESS != rc) 
        {
            ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
            ESP_LOGE(TAG, " client id %s", connectParams.pClientID );
            vTaskDelay(10000 / portTICK_RATE_MS);
        }
    } while(SUCCESS != rc);

    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if(SUCCESS != rc) 
    {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }


    ESP_LOGI(TAG, "Subscribing to %s...", TOPIC);
    rc = aws_iot_mqtt_subscribe(&client, 
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

    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = (void *) cPayload;
    paramsQOS0.isRetained = 0;

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

        if(xQueueReceive( xmitQueue, &msg, 1000 ))
        {
            (*(msg->func))();
            delete msg;

        }
    }

    ESP_LOGE(TAG, "An error occurred in the main loop.");
    abort();
}

uint8_t sendMQTTmsg(cJSON *s)
{

    auto f = [=] () 
        {
            if (wifi_operational == false)
            {
                printf("WIFI not operational dropping mqtt msg\n");
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
    return xQueueSend(xmitQueue, &mr, 0);
}

void wifiConnected()
{
    auto f = [=] () 
        {
            wifi_operational = true;
        };

    auto mr = new MRequest(NULL, f);
    xQueueSend(xmitQueue, &mr, 0);
}

void wifiDisconnected()
{
    auto f = [=] () 
        {
            wifi_operational = false;
        };

    auto mr = new MRequest(NULL, f);
    xQueueSend(xmitQueue, &mr, 0);
}

extern "C" void app_main();


WWC *aWWC;
WiFi *aWiFi;

void createActiveObjects()
{
    aWWC = new WWC();
    aWiFi = new WiFi();
}


void app_main()
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 9216, NULL, 5, NULL, 1);
    createActiveObjects();
}

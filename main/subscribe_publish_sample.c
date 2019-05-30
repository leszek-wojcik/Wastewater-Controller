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

static const char *TAG = "subpub";

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
extern const uint8_t certificate_pem_crt_start[] asm("_binary_certificate_pem_crt_start");
extern const uint8_t certificate_pem_crt_end[] asm("_binary_certificate_pem_crt_end");
extern const uint8_t private_pem_key_start[] asm("_binary_private_pem_key_start");
extern const uint8_t private_pem_key_end[] asm("_binary_private_pem_key_end");

// AWS IoT client endpoint
const char *client_endpoint = "acpzbsqyyfpu4.iot.eu-west-2.amazonaws.com";

// AWS IoT device certificate (ECC)
const char *client_cert =
"-----BEGIN CERTIFICATE-----\r\n"
"MIICoTCCAYmgAwIBAgIUJ5o0lwZlj+SD4Sa/LPYrsXbz0mUwDQYJKoZIhvcNAQEL\r\n"
"BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\r\n"
"SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTE5MDIyNzIwNDU1\r\n"
"MVoXDTQ5MTIzMTIzNTk1OVowMTELMAkGA1UEBhMCUEwxEzARBgNVBAgMClNvbWUt\r\n"
"U3RhdGUxDTALBgNVBAoMBHRlc3QwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQq\r\n"
"c+0lympsxsdBlwNv0/6ZfC0WxsseBNjJrntrqvhV4K72BPsg+cASFxHzsIgq1jxS\r\n"
"V7+UMYbzsOE8lLp2kSj0o2AwXjAfBgNVHSMEGDAWgBRRMQiWgq1LqUb8mgHoFwhy\r\n"
"wBTtcDAdBgNVHQ4EFgQU37fGHXJB0RA+6tAuXd9X921O00owDAYDVR0TAQH/BAIw\r\n"
"ADAOBgNVHQ8BAf8EBAMCB4AwDQYJKoZIhvcNAQELBQADggEBAE+mW3JNsTcfwdQQ\r\n"
"2S1GUOmQEWV6eQ2bzQHMTyZfEJQIh8VBW4APhxKMP1tD/KXJ8fKImVFDIXEDVif7\r\n"
"ollAs8P+lJz6Tii4/fKo90AGytaJJALyBWkskKFQ/2QAqcWfPwUP5rriM4BKat5O\r\n"
"Lfr5emGr0Y174LU4JNkaK+H9o/aFttGpUaY6RjHeWbZ5X6DM5KJzE3GbelI4vv0Z\r\n"
"geKjhQXvaVrwiVc+TB9cxUKNoFKeEgaXMjxsFzvDYo4EtQXkTIlM1mwxgMd467R3\r\n"
"PMT7cgy1qPibAD031pjZEvLi5sMoDO+KP6C/T0mpNq2mkiCZq6wrwgz8cIUdbJd/\r\n"
"4khOuvY=\r\n"
"-----END CERTIFICATE-----\r\n";

// AWS IoT device private key (ECC)
const char *client_key = 
"-----BEGIN EC PARAMETERS-----\r\n"
"BggqhkjOPQMBBw==\r\n"
"-----END EC PARAMETERS-----\r\n"
"-----BEGIN EC PRIVATE KEY-----\r\n"
"MHcCAQEEIA5lYX2aP14ijN1oOtQFdCm867oTmlA/cPUFILDkr9hfoAoGCCqGSM49\r\n"
"AwEHoUQDQgAEKnPtJcpqbMbHQZcDb9P+mXwtFsbLHgTYya57a6r4VeCu9gT7IPnA\r\n"
"EhcR87CIKtY8Ule/lDGG87DhPJS6dpEo9A==\r\n"
"-----END EC PRIVATE KEY-----\r\n";

const char *ca_cert = "-----BEGIN CERTIFICATE-----\r\n"
        "MIIE0zCCA7ugAwIBAgIQGNrRniZ96LtKIVjNzGs7SjANBgkqhkiG9w0BAQUFADCB\r\n"
        "yjELMAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQL\r\n"
        "ExZWZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJp\r\n"
        "U2lnbiwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxW\r\n"
        "ZXJpU2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0\r\n"
        "aG9yaXR5IC0gRzUwHhcNMDYxMTA4MDAwMDAwWhcNMzYwNzE2MjM1OTU5WjCByjEL\r\n"
        "MAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQLExZW\r\n"
        "ZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJpU2ln\r\n"
        "biwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxWZXJp\r\n"
        "U2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0aG9y\r\n"
        "aXR5IC0gRzUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCvJAgIKXo1\r\n"
        "nmAMqudLO07cfLw8RRy7K+D+KQL5VwijZIUVJ/XxrcgxiV0i6CqqpkKzj/i5Vbex\r\n"
        "t0uz/o9+B1fs70PbZmIVYc9gDaTY3vjgw2IIPVQT60nKWVSFJuUrjxuf6/WhkcIz\r\n"
        "SdhDY2pSS9KP6HBRTdGJaXvHcPaz3BJ023tdS1bTlr8Vd6Gw9KIl8q8ckmcY5fQG\r\n"
        "BO+QueQA5N06tRn/Arr0PO7gi+s3i+z016zy9vA9r911kTMZHRxAy3QkGSGT2RT+\r\n"
        "rCpSx4/VBEnkjWNHiDxpg8v+R70rfk/Fla4OndTRQ8Bnc+MUCH7lP59zuDMKz10/\r\n"
        "NIeWiu5T6CUVAgMBAAGjgbIwga8wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8E\r\n"
        "BAMCAQYwbQYIKwYBBQUHAQwEYTBfoV2gWzBZMFcwVRYJaW1hZ2UvZ2lmMCEwHzAH\r\n"
        "BgUrDgMCGgQUj+XTGoasjY5rw8+AatRIGCx7GS4wJRYjaHR0cDovL2xvZ28udmVy\r\n"
        "aXNpZ24uY29tL3ZzbG9nby5naWYwHQYDVR0OBBYEFH/TZafC3ey78DAJ80M5+gKv\r\n"
        "MzEzMA0GCSqGSIb3DQEBBQUAA4IBAQCTJEowX2LP2BqYLz3q3JktvXf2pXkiOOzE\r\n"
        "p6B4Eq1iDkVwZMXnl2YtmAl+X6/WzChl8gGqCBpH3vn5fJJaCGkgDdk+bW48DW7Y\r\n"
        "5gaRQBi5+MHt39tBquCWIMnNZBU4gcmU7qKEKQsTb47bDN0lAtukixlE0kF6BWlK\r\n"
        "WE9gyn6CagsCqiUXObXbf+eEZSqVir2G3l6BFoMtEMze/aiCKm0oHw0LxOXnGiYZ\r\n"
        "4fQRbxC1lfznQgUy286dUV4otp6F01vvpX1FQHKOtw5rDgb7MzVIcbidJ4vEZV8N\r\n"
        "hnacRHr2lVz2XTIIM6RUthg/aFzyQkqFOFSDX9HoLPKsEdao7WNq\r\n"
        "-----END CERTIFICATE-----\r\n";
/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;


/* CA Root certificate, device ("Thing") certificate and device
 * ("Thing") key.

   Example can be configured one of two ways:

   "Embedded Certs" are loaded from files in "certs/" and embedded into the app binary.

   "Filesystem Certs" are loaded from the filesystem (SD card, etc.)

   See example README for more details.
*/


/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

void aws_iot_task(void *param) {
    char cPayload[100];

    int32_t i = 0;

    IoT_Error_t rc = FAILURE;

    AWS_IoT_Client client;
    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

    IoT_Publish_Message_Params paramsQOS0;
    IoT_Publish_Message_Params paramsQOS1;

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    mqttInitParams.enableAutoReconnect = false; // We enable this later below
    mqttInitParams.pHostURL = client_endpoint;
    mqttInitParams.port = port;

    //mqttInitParams.pRootCALocation = ca_cert;
    //mqttInitParams.pDeviceCertLocation = client_cert;
    //mqttInitParams.pDevicePrivateKeyLocation = client_key;

    mqttInitParams.pRootCALocation = (const char*) aws_root_ca_pem_start;
    mqttInitParams.pDeviceCertLocation = (const char*) certificate_pem_crt_start; 
    mqttInitParams.pDevicePrivateKeyLocation = (const char*) private_pem_key_start;

    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnectCallbackHandler;
    mqttInitParams.disconnectHandlerData = NULL;


    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    connectParams.keepAliveIntervalInSec = 10;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;
    /* Client ID is set in the menuconfig of the example */
    connectParams.pClientID = CONFIG_AWS_EXAMPLE_CLIENT_ID;
    connectParams.clientIDLen = (uint16_t) strlen(CONFIG_AWS_EXAMPLE_CLIENT_ID);
    connectParams.isWillMsgPresent = false;

    ESP_LOGI(TAG, "Connecting to AWS...");
    do {
        rc = aws_iot_mqtt_connect(&client, &connectParams);
        if(SUCCESS != rc) {
            ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } while(SUCCESS != rc);

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }

    const char *TOPIC = "dupa";
    const int TOPIC_LEN = strlen(TOPIC);

    ESP_LOGI(TAG, "Subscribing...");
    rc = aws_iot_mqtt_subscribe(&client, TOPIC, TOPIC_LEN, QOS0, iot_subscribe_callback_handler, NULL);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Error subscribing : %d ", rc);
        abort();
    }

    sprintf(cPayload, "%s : %d ", "hello from SDK", i);

    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = (void *) cPayload;
    paramsQOS0.isRetained = 0;

    paramsQOS1.qos = QOS1;
    paramsQOS1.payload = (void *) cPayload;
    paramsQOS1.isRetained = 0;

    while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)) {

        //Max time the yield function will wait for read messages
        rc = aws_iot_mqtt_yield(&client, 100);
        if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            // If the client is attempting to reconnect we will skip the rest of the loop.
            continue;
        }

        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(1000 / portTICK_RATE_MS);
        sprintf(cPayload, "%s : %d ", "hello from ESP32 (QOS0)", i++);
        paramsQOS0.payloadLen = strlen(cPayload);
        rc = aws_iot_mqtt_publish(&client, TOPIC, TOPIC_LEN, &paramsQOS0);

        sprintf(cPayload, "%s : %d ", "hello from ESP32 (QOS1)", i++);
        paramsQOS1.payloadLen = strlen(cPayload);
        rc = aws_iot_mqtt_publish(&client, TOPIC, TOPIC_LEN, &paramsQOS1);
        if (rc == MQTT_REQUEST_TIMEOUT_ERROR) {
            ESP_LOGW(TAG, "QOS1 publish ack not received.");
            rc = SUCCESS;
        }
    }

    ESP_LOGE(TAG, "An error occurred in the main loop.");
    abort();
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
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

    initialise_wifi();
    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 9216, NULL, 5, NULL, 1);
}

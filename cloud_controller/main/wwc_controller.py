import json
import time
import datetime 
import dateutil.parser
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

def customCallback(client, userdata, message):
    print("Received a new message: ")
    print(message.payload)
    print("from topic: ")
    print(message.topic)
    print("--------------\n\n")

injason = """
{
  "time": "2016-12-30T18:44:49Z",
  "detail": {}
}
"""
myMQTTClient = AWSIoTMQTTClient("wwc_controller")
myMQTTClient.configureEndpoint("acpzbsqyyfpu4-ats.iot.eu-west-1.amazonaws.com", 8883)
myMQTTClient.configureCredentials("certs/aws-root-ca.pem",
                                  "certs/527a5b73d5-private.pem.key",
                                  "certs/527a5b73d5-certificate.pem.crt") 

myMQTTClient.configureAutoReconnectBackoffTime(1, 32, 20)
myMQTTClient.configureOfflinePublishQueueing(-1)
myMQTTClient.configureDrainingFrequency(2)
myMQTTClient.configureConnectDisconnectTimeout(10)
myMQTTClient.configureMQTTOperationTimeout(5)
myMQTTClient.connect()
myMQTTClient.subscribe("wwc/myesp32/status", 1, customCallback)
myMQTTClient.subscribe("$aws/events/subscriptions/subscribed/myesp32", 1, customCallback)

event = json.loads(injason)
d = dateutil.parser.parse (event["time"])

controlMessageTopic = "wwc/myesp32/control"
message = dict()
message["autoMode"] = True
message["areation"] = False
message["circulation"] = True
message["wwcCounter"] = 15
message["requestStatus"] = True

shadowMessage = dict()
shadowMessage["autoMode"] = True
shadowMessage["areation"] = True
shadowMessage["circulation"] = True
shadowMessage["areationFail"] = False
shadowMessage["circulationFail"] = False

#myMQTTClient.publish(controlMessageTopic, json.dumps(message), 1)

while(True):
    time.sleep(2)

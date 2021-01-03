import json
import time
import datetime 
import dateutil.parser
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient

controlMessageTopic = "wwc/esp32/control"
shadowGetTopic = "$aws/things/esp32/shadow/get"
shadowGetRejectedTopic = "$aws/things/esp32/shadow/get/rejected"
shadowGetAcceptedTopic = "$aws/things/esp32/shadow/get/accepted"
shadowUpdateTopic = "$aws/things/esp32/shadow/update"

def customCallback(client, userdata, message):
    print(datetime.datetime.now())
    print( "----" )
    print( userdata )
    print( "----" )
    print( message.payload.decode("utf-8"))
   # print( json.loads( message.payload.decode("utf-8") ))

injason = """
{
  "time": "2016-12-30T18:44:49Z",
  "detail": {}
}
"""
myMQTTClient = AWSIoTMQTTClient("wwc_controller")
myMQTTClient.configureEndpoint("acpzbsqyyfpu4-ats.iot.eu-west-1.amazonaws.com", 8883)
myMQTTClient.configureCredentials("certs/root.ca.pem",
                                  "certs/private.key.pem",
                                  "certs/certificate.pem") 

myMQTTClient.configureAutoReconnectBackoffTime(1, 32, 20)
myMQTTClient.configureOfflinePublishQueueing(-1)
myMQTTClient.configureDrainingFrequency(2)
myMQTTClient.configureConnectDisconnectTimeout(10)
myMQTTClient.configureMQTTOperationTimeout(5)
myMQTTClient.connect()
#myMQTTClient.subscribe("wwc/myesp32/status", 1, customCallback)
#myMQTTClient.subscribe("$aws/events/subscriptions/subscribed/myesp32", 1, customCallback)
#myMQTTClient.subscribe(shadowGetTopic, 1, customCallback)
#myMQTTClient.subscribe(shadowGetRejectedTopic, 1, customCallback)
myMQTTClient.subscribe(shadowGetAcceptedTopic, 1, customCallback)

event = json.loads(injason)
d = dateutil.parser.parse (event["time"])

message = dict()
message["state"] = dict ()
message["state"]["desired"] = dict ()
message["state"]["desired"]["normalPeriod"] = 5 * 60 * 1000 		# 10 minutes
message["state"]["desired"]["normalDutyCycle"] = 0.30       		# 90% dutycycle
message["state"]["desired"]["circulationPeriod"] = 10 * 60 * 1000       # 10 minutes
message["state"]["desired"]["pumpFailureThreshold"] = 200
message["state"]["desired"]["circulationStartGMTOffset"] = -60 * 60 * 1000 # 1 hour

emptyMessage = dict()
#myMQTTClient.publish(controlMessageTopic, json.dumps(message), 1)
print ( "Publising to " + shadowGetTopic + "\n")
#print ( json.dumps(message) )
#myMQTTClient.publish(shadowUpdateTopic, json.dumps(message) , 1)
myMQTTClient.publish(shadowUpdateTopic, json.dumps(message) , 1)

while(True):
    time.sleep(5)

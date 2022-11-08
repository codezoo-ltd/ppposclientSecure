#include "secrets.h"
#include <ArduinoJson.h>
#include <PPPOSClientSecure.h>
#include <PPPOSSecure.h>
#include <PubSubClient.h>

#include "TYPE1SC.h"

#define SERIAL_BR 115200
#define GSM_SERIAL 1
#define GSM_RX 16
#define GSM_TX 17
#define GSM_BR 115200

#define PWR_PIN 5
#define RST_PIN 18
#define WAKEUP_PIN 19

#define DebugSerial Serial
#define M1Serial Serial2 // ESP32

#define TESTCOUNT 10 // MQTT Publish test count

// <myiothub> : My Azure IoT Hub Name
// <myDevice> : My Azrue IoT Device Name

const char *iothub_url = "<myiothub>.azure-devices.net";

const char *iothub_deviceid = "<myDevice>";

const char *iothub_user = "<myiothub>.azure-devices.net/<myDevice>";

const char *iothub_sas_token =
    "SharedAccessSignature "
    "sr=<myiothub>.azure-devices.net%2Fdevices%2F<myDevice>&sig="
    "123&se=456";

// default topic feed for subscribing
const char *iothub_subscribe_endpoint =
    "devices/<myDevice>/messages/devicebound/#";

// default topic feed for publishing
const char *iothub_publish_endpoint = "devices/<myDevice>/messages/events/";

char *ppp_user = "codezoo";
char *ppp_pass = "codezoo";
String APN = "simplio.apn";

PPPOSClientSecure espClient;
PubSubClient client(espClient);

long lastMsg = 0;
int count = TESTCOUNT;

TYPE1SC TYPE1SC(M1Serial, DebugSerial, PWR_PIN, RST_PIN, WAKEUP_PIN);

/* callback */
// callback function for when a message is dequeued from the MQTT server
void callback(char *topic, byte *payload, unsigned int length) {
  // print message to serial for debugging
  Serial.print("Message arrived: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
}

bool startPPPOS() {
  PPPOS_start();
  unsigned long _tg = millis();
  while (!PPPOS_isConnected()) {
    DebugSerial.println("ppp Ready...");
    if (millis() > (_tg + 30000)) {
      PPPOS_stop();
      return false;
    }
    delay(3000);
  }

  DebugSerial.println("PPPOS Started");
  return true;
}

// function to connect to MQTT server
void connect_mqtt() {
  // Loop until we're reconnected

  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(iothub_deviceid, iothub_user, iothub_sas_token)) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(iothub_subscribe_endpoint);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  /* CATM1 Modem PowerUp sequence */
  pinMode(PWR_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(WAKEUP_PIN, OUTPUT);

  digitalWrite(PWR_PIN, HIGH);
  digitalWrite(WAKEUP_PIN, HIGH);
  digitalWrite(RST_PIN, LOW);
  delay(100);
  digitalWrite(RST_PIN, HIGH);
  delay(2000);
  /********************************/
  // put your setup code here, to run once:
  M1Serial.begin(SERIAL_BR);
  DebugSerial.begin(SERIAL_BR);

  DebugSerial.println("TYPE1SC Module Start!!!");

  /* TYPE1SC Module Initialization */
  if (TYPE1SC.init()) {
    DebugSerial.println("TYPE1SC Module Error!!!");
  }

  /* Network Regsistraiton Check */
  while (TYPE1SC.canConnect() != 0) {
    DebugSerial.println("Network not Ready !!!");
    delay(2000);
  }

  /* Get Time (GMT, (+36/4) ==> Korea +9hour) */
  char szTime[32];
  if (TYPE1SC.getCCLK(szTime, sizeof(szTime)) == 0) {
    DebugSerial.print("Time : ");
    DebugSerial.println(szTime);
  }
  delay(1000);

  int rssi, rsrp, rsrq, sinr;
  for (int i = 0; i < 3; i++) {
    /* Get RSSI */
    if (TYPE1SC.getRSSI(&rssi) == 0) {
      DebugSerial.println("Get RSSI Data");
    }
    delay(1000);

    /* Get RSRP */
    if (TYPE1SC.getRSRP(&rsrp) == 0) {
      DebugSerial.println("Get RSRP Data");
    }
    delay(1000);

    /* Get RSRQ */
    if (TYPE1SC.getRSRQ(&rsrq) == 0) {
      DebugSerial.println("Get RSRQ Data");
    }
    delay(1000);

    /* Get SINR */
    if (TYPE1SC.getSINR(&sinr) == 0) {
      DebugSerial.println("Get SINR Data");
    }
    delay(1000);
    int count = 3 - (i + 1);
    DebugSerial.print(count);
    DebugSerial.println(" left..");
  }

  if (TYPE1SC.setPPP() == 0) {
    DebugSerial.println("PPP mode change");
  }

  String RF_STATUS = "[RF Status] RSSI: " + String(rssi) +
                     " RSRP:" + String(rsrp) + " RSRQ:" + String(rsrq) +
                     " SINR:" + String(sinr);
  DebugSerial.println(RF_STATUS);
  DebugSerial.println("TYPE1SC Module Ready!!!");

  pinMode(25, OUTPUT);

  /* PPPOS Setup */
  PPPOS_init(GSM_TX, GSM_RX, GSM_BR, GSM_SERIAL, ppp_user, ppp_pass);
  DebugSerial.println("Starting PPPOS...");

  if (startPPPOS()) {
    DebugSerial.println("Starting PPPOS... OK");
  } else {
    DebugSerial.println("Starting PPPOS... Failed");
  }

  client.setCallback(callback);
  espClient.setCACert(AZURE_CERT_CA);
  client.setServer(iothub_url, 8883);
}

void loop() {
  if (!client.connected()) {
    connect_mqtt();
  }
  client.loop();
  long now = millis();

  // publish data and debug mqtt connection every 10 seconds
  if (now - lastMsg > 10000 && count > 0) {
    lastMsg = now;

    Serial.print("is MQTT client is still connected: ");
    Serial.println(client.connected());

    // set up json object
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    // populate keys in json
    root["sensor"] = "moisture";
    // substitute the int value below for a real sensor reading (ie. an
    // analogRead() result)
    root["data"] = 128;

    // convert json to buffer for publishing
    char buffer[256];
    root.printTo(buffer, sizeof(buffer));

    // publish
    client.publish(iothub_publish_endpoint, buffer);
    count--;
  }
}

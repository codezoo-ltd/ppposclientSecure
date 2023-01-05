#include <Arduino.h>
#include <AzureIoTCentral.h>
#include <Azure_CA.h>
#include <ESP32Servo.h>
#include <PPPOSClientSecure.h>
#include <PPPOSSecure.h>

Servo myservo; // create servo object to control a servo
int pos = 0;   // variable to store the servo position
int servoPin = 27;

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

char *ppp_user = "codezoo";
char *ppp_pass = "codezoo";
String APN = "simplio.apn";

#define BATT_REPORT 3600000 // 1hour
//#define BATT_REPORT 600000  //10min
//#define BATT_REPORT 10000  //10sec
#define BATT_ADC 25
#define AVGCNT 30

PPPOSClientSecure espClient;

float readBattery() {
  uint8_t samples = AVGCNT;
  float array[AVGCNT];
  float batt_adc = 0;

  for (int i = 0; i < samples; i++) {
    int vref = 1100;
    uint16_t volt = analogRead(BATT_ADC);
    float battery_voltage = ((float)volt / 4095.0) * 2.0 * 3.3 * (vref);

    array[i] = battery_voltage;
    delay(10);
  }

  std::sort(array, array + samples);
  for (int i = 0; i < samples; i++) {
    if (i == 0 || i == samples - 1)
      continue;
    batt_adc += array[i];
  }

  batt_adc /= samples - 2;
  return batt_adc;
}

void servoInit() {
  myservo.attach(servoPin, 1000,
                 2000); // attaches the servo on pin 27 to the servo object
  // using default min/max of 1000us and 2000us
  // different servos may require different min/max settings
  // for an accurate 0 to 180 sweep
  for (pos = 0; pos <= 70; pos += 1) { // goes from 0 degrees to 70 degrees
    // in steps of 1 degree
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(5);           // waits 5ms for the servo to reach the position
  }
  myservo.detach();
}

void servoUp() {
  myservo.attach(servoPin, 1000,
                 2000); // attaches the servo on pin 27 to the servo object
  // using default min/max of 1000us and 2000us
  // different servos may require different min/max settings
  // for an accurate 0 to 180 sweep
  for (pos = 70; pos <= 120; pos += 1) { // goes from 70 degrees to 120 degrees
    // in steps of 1 degree
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(5);           // waits 5ms for the servo to reach the position
  }
  for (pos = 120; pos >= 40; pos -= 1) { // goes from 120 degrees to 40 degrees
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(5);           // waits 5ms for the servo to reach the position
  }
  myservo.detach();
}

void servoDown() {
  myservo.attach(servoPin, 1000,
                 2000); // attaches the servo on pin 27 to the servo object
  for (pos = 0; pos <= 70; pos += 1) { // goes from 0 degrees to 70 degrees
    // in steps of 1 degree
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(5);           // waits 5ms for the servo to reach the position
  }
  myservo.detach();
}

time_t getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  time(&now);
  return now;
}

AzureIoTCentral iot(espClient, getTime);

TYPE1SC TYPE1SC(M1Serial, DebugSerial, PWR_PIN, RST_PIN, WAKEUP_PIN);

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

  if (TYPE1SC.setPPP() == 0) {
    DebugSerial.println("PPP mode change");
  }

  espClient.setCACert(AZURE_ROOT_CA);

  /* PPPOS Setup */
  PPPOS_init(GSM_TX, GSM_RX, GSM_BR, GSM_SERIAL, ppp_user, ppp_pass);
  DebugSerial.println("Starting PPPOS...");

  if (startPPPOS()) {
    DebugSerial.println("Starting PPPOS... OK");
  } else {
    DebugSerial.println("Starting PPPOS... Failed");
  }

  iot.configs("", // ID scope
              "", // Device ID
              ""  // Primary key
  );

  iot.addCommandHandle(
      "KeyUnlock", [](String payload) { // Add handle 'KeyUnlock' command
        int valueInt =
            payload.toInt(); // Convart payload in String to Number (int)
        servoDown();
        delay(1000);
        Serial.println("KeyUnlock");

        iot.setTelemetryValue("KeyStatus", false);

        if (iot.sendMessage()) {
          Serial.println("Send Response");
        } else {
          Serial.println("Send Response error!");
        }
      });

  iot.addCommandHandle(
      "KeyLock", [](String payload) { // Add handle 'KeyLock' command
        int valueInt =
            payload.toInt(); // Convart payload in String to Number (int)
        servoUp();
        delay(1000);
        Serial.println("KeyLock");

        iot.setTelemetryValue("KeyStatus", true);

        if (iot.sendMessage()) {
          Serial.println("Send Response");
        } else {
          Serial.println("Send Response error!");
        }
      });

  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50); // standard 50 hz servo
  servoInit();

  // Config NTP server
  configTime(0, 0, "pool.ntp.org");
}

void loop() {
  if (iot.isConnected()) {
    iot.loop();

    static uint32_t startTime = -5000;
    if ((millis() - startTime) >= BATT_REPORT) {
      startTime = millis();
      float battery_voltage = readBattery();
      iot.setTelemetryValue("Battery", battery_voltage);

      if (iot.sendMessage()) { // Send telemetry to Azure IoT Central
        Serial.println("Send!");
        Serial.print(battery_voltage);
        Serial.println("");
      } else {
        Serial.println("Send error!");
      }
    }

  } else {
    Serial.println("iot Connect Ready...");
    iot.connect();
    Serial.println("iot Connect Complete!");
  }
  delay(1);
}

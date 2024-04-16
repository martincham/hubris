#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define IN2 18        // white  motor control
#define IN1 17        // gray   motor control
#define TOP 6         // purple limit switch
#define BOTTOM 5      // red    limit switch
#define ENCA 1        // white  motor encoder
#define ENCB 3        // yellow motor encoder
#define IN1CHANNEL 1  //PMW Channel
#define IN2CHANNEL 2  // PMW Channel

// Received from queen bee
int sleepTime = 2;    // seconds
int fastSpeed = 100;  // PMW out of 255
int slowSpeed = 80;   // PMW out of 255

#include <PWMOutESP32.h>  //i dont know why i still need this but it breaks without

PWMOutESP32 pwm;

#include "RTClib.h"  // I don't know why I need this anymore, but it breaks without it
RTC_DS3231 rtc;

#include <Adafruit_I2CDevice.h>
#include "NTP.h"


int botSwitch = 1;  // 0 when triggered
int topSwitch = 1;  // 0 when triggered

int direction = 0;  //1 is up, -1 down, 0 not moving
unsigned long previousMillis = 0;
const long interval = 20;

unsigned long long uS_TO_S_FACTOR = 1000000; /* Conversion factor for micro seconds to seconds */

volatile long motorPosition = -35000;
int diffPosition;
int prevPosition;
int resolution = 8;
int frequency = 20000;

void IRAM_ATTR doEncoderA();
void IRAM_ATTR doEncoderB();
void insideLoop();
void printTime();
void stopMotor();
void goUp(int slow = 0);
void goDown(int slow = 0);


// ESP-NOW Receiving Code
typedef struct struct_message {
  int a;  // direction
  int b;  // sleep time
  int c;  // fast speed
  int d;  //slow speed
} struct_message;

struct_message espNowMessage;

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&espNowMessage, incomingData, sizeof(espNowMessage));
  direction = espNowMessage.a;
  sleepTime = min(espNowMessage.b - 10, 0);
  fastSpeed = espNowMessage.c;
  slowSpeed = espNowMessage.d;
  Serial.print("Direction: ");
  Serial.println(direction);
  Serial.print("Sleep Time: ");
  Serial.println(sleepTime);
  Serial.print("Fast: ");
  Serial.println(fastSpeed);
  Serial.print("Slow: ");
  Serial.println(slowSpeed);
  delay(4000);
}



void setup() {
  Serial.begin(57600);
  Serial.println("Setup");

  pinMode(TOP, INPUT_PULLUP);  // limit switches
  pinMode(BOTTOM, INPUT_PULLUP);
  attachInterrupt(ENCA, doEncoderA, CHANGE);
  attachInterrupt(ENCB, doEncoderB, CHANGE);

  pinMode(IN2, OUTPUT);  //  motor PWMs
  pinMode(IN1, OUTPUT);
  ledcAttachPin(IN1, IN1CHANNEL);
  ledcSetup(IN1CHANNEL, frequency, resolution);
  ledcAttachPin(IN2, IN2CHANNEL);
  ledcSetup(IN2CHANNEL, frequency, resolution);

  // initializing the rtc
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    Serial.flush();
    while (1) delay(10);
  }
  // Long sleep when gallery is closed. Run here to not setup wifi when unneeded
  int longSleep = calculateSleepDuration();
  if (longSleep != 0){
    goToDeepSleep(longSleep);
  }

  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  // Initilize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);
}


void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    insideLoop();
  }
}

void insideLoop() {
  botSwitch = digitalRead(BOTTOM);
  topSwitch = digitalRead(TOP);

  printTime();
  Serial.print(motorPosition);
  Serial.print(", ");
  diffPosition = abs(motorPosition - prevPosition);  // calc change over time and make it always position with abs
  prevPosition = motorPosition;
  Serial.print(diffPosition);
  Serial.print(", ");
  Serial.print("bot=");
  Serial.print(botSwitch);
  Serial.print(", top=");
  Serial.print(topSwitch);
  Serial.print(", direction=");
  Serial.print(direction);
  Serial.print(", sleep=");
  Serial.print(sleepTime);
  Serial.print(", fast=");
  Serial.print(fastSpeed);
  Serial.print(", slow=");
  Serial.println(slowSpeed);





  if (direction == 0) {
    // Wating on ESP-NOW, do nothing until direction is set
    return;
  }
  /*
  if (direction == 0 && topSwitch == 0){
    direction = -1; 
    Serial.println("Awoke, going down.");
  } 
  if (direction == 0 && topSwitch == 1){
    direction = 1;
    Serial.println("Awoke, going up.");
  }
  */


  if (topSwitch == 0 && direction == 1) {
    // set down
    direction = -1;
    stopMotor();
    Serial.println("hit top");
    motorPosition = 0;
    prevPosition = 0;

    esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_S_FACTOR);
    Serial.println("Entering deep sleep mode...");
    esp_deep_sleep_start();  // Start the deep sleep mode

  } else if (botSwitch == 0 && direction == -1) {
    //set up
    direction = 1;
    stopMotor();
    Serial.println("hit bottom");

    esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_S_FACTOR);
    Serial.println("Entering deep sleep mode...");
    esp_deep_sleep_start();  // Start the deep sleep mode
  }

  if (direction == 1) {
    // go up
    goUp(1);  // go slowly
  } else if (direction == -1) {
    //go down
    goDown();
  }
}


// ENCODER FUNCTIONS

void IRAM_ATTR doEncoderA() {

  // look for a low-to-high on channel A
  if (digitalRead(ENCA) == HIGH) {
    // check channel B to see which way encoder is turning
    if (digitalRead(ENCB) == LOW) {
      motorPosition = motorPosition + 1;  // CW
    } else {
      motorPosition = motorPosition - 1;  // CCW
    }
  } else  // must be a high-to-low edge on channel A
  {
    // check channel B to see which way encoder is turning
    if (digitalRead(ENCB) == HIGH) {
      motorPosition = motorPosition + 1;  // CW
    } else {
      motorPosition = motorPosition - 1;  // CCW
    }
  }
}

void IRAM_ATTR doEncoderB() {

  // look for a low-to-high on channel B
  if (digitalRead(ENCB) == HIGH) {
    // check channel A to see which way encoder is turning
    if (digitalRead(ENCA) == HIGH) {
      motorPosition = motorPosition + 1;  // CW
    } else {
      motorPosition = motorPosition - 1;  // CCW
    }
  }
  // Look for a high-to-low on channel B
  else {
    // check channel B to see which way encoder is turning
    if (digitalRead(ENCA) == LOW) {
      motorPosition = motorPosition + 1;  // CW
    } else {
      motorPosition = motorPosition - 1;  // CCW
    }
  }
}

void printTime() {
  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}


// MOTOR FUNCTIONS

void stopMotor() {
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, 0);
}
void goUp(int slow) {
  int speed;
  if (slow == 1) {
    speed = slowSpeed;
  } else {
    speed = fastSpeed;
  }
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, speed);
  Serial.print(" power=");
  Serial.print(speed);
}
void goDown(int slow) {
  int speed;
  if (slow == 1) {
    speed = slowSpeed;
  } else {
    speed = fastSpeed;
  }
  ledcWrite(IN1CHANNEL, speed);
  ledcWrite(IN2CHANNEL, 0);
}

void goToDeepSleep(int sleepDuration) {
  esp_sleep_enable_timer_wakeup(sleepDuration * uS_TO_S_FACTOR);
  Serial.println("Entering deep sleep mode...");
  esp_deep_sleep_start();
}

// Returns time to sleep, or 0 is shouldn't sleep
int calculateSleepDuration(){
  DateTime now = rtc.now();
  int dayOfTheWeek = now.dayOfTheWeek();
  int hour = now.hour();
  int minute = now.minute();

  // SLEEP SCHEDULE // * 60 seconds for minutes
  
  if (dayOfTheWeek >= 3) { // Wednesday through Saturday
    if ( hour >= 12 && hour <= 18) { // Time to stay awake
      return 0;
    }
    if ( hour <= 8 || hour >= 18) {
      return 180 * 60; // three hours 
    }
    if ( hour < 12) { // sleep until 12
      int hourUntil12 = 11 - hour;
      int minuteUntilHour = 59 - minute;
      return ((hourUntil12 * 60) + minuteUntilHour) * 60;
    }
  }
  else{ // Sunday through Tuesday
    return 180 * 60; // three hours
  } 
  return 0;
}

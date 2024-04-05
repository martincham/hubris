#include <Arduino.h>

#define IN2 18      // white
#define IN1 17      // gray
#define TOP 6     // purple
#define BOTTOM 5  // red
#define ENCA 1    // white
#define ENCB 3    // yellow
#define IN1CHANNEL 1
#define IN2CHANNEL 2

#define SLEEP_TIME 4 // 1 minute
#define FAST_SPEED 150
#define SLOW_SPEED 20

#include <PWMOutESP32.h>
PWMOutESP32 pwm;

#include "RTClib.h"
RTC_DS3231 rtc;

int botSwitch = 1;  // 0 when triggered
int topSwitch = 1;  // 0 when triggered

int direction = 1;  //1 is up, -1 down
unsigned long previousMillis = 0;
const long interval = 20;

volatile long motorPosition = 0;
int diffPosition;
int prevPosition;
int resolution = 8;
int frequency = 20000;

void IRAM_ATTR doEncoderA();
void IRAM_ATTR doEncoderB();
void insideLoop();
void printTime();
void stop();
void goUp(int fast = 0):
void goDown(int fast = 0);


void setup() {
  Serial.begin(57600);

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
    if(!rtc.begin()) {
        Serial.println("Couldn't find RTC!");
        Serial.flush();
        while (1) delay(10);
    }

    if(rtc.lostPower()) {
        // this will adjust to the date and time at compilation
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
}


void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    insideLoop();
  }
}

void insideLoop(){
  botSwitch = digitalRead(BOTTOM);
  topSwitch = digitalRead(TOP);

  printTime();
  Serial.print(motorPosition);
  Serial.print(", ");
  diffPosition = abs(motorPosition - prevPosition);  // calc change over time and make it always position with abs
  prevPosition = motorPosition;
  Serial.print(diffPosition);
  Serial.print(", ");

  if (topSwitch == 0 && direction == 1) {
    // set down
    direction = -1;
    ledcWrite(IN1CHANNEL, 0);
    ledcWrite(IN2CHANNEL, 0);
    Serial.println("hit top");
    motorPosition = 0;
    prevPosition = 0;
    delay(SLEEP_TIME);
  }
  if (botSwitch == 0 && direction == -1) {
    //set up
    direction = 1;
    ledcWrite(IN1CHANNEL, 0);
    ledcWrite(IN2CHANNEL, 0);
    Serial.println("hit bottom");
    delay(SLEEP_TIME);
  }
  if (direction == 1) {
    // go up
    ledcWrite(IN1CHANNEL, 0);
    ledcWrite(IN2CHANNEL, 150);;
  } else {
    //go down
    ledcWrite(IN1CHANNEL, 150);
    ledcWrite(IN2CHANNEL, 0);
  }

  Serial.print("bot=");
  Serial.print(botSwitch);
  Serial.print(", top=");
  Serial.print(topSwitch);
  Serial.print(", direction=");

  Serial.println(direction);
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

void printTime(){
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

void stop(){
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, 0);
}
void goUp(int fast = 0){
  int speed;
  if( fast == 1) {
    speed = FAST_SPEED;
  }
  else{
    speed = SLOW_SPEED;
  }
  ledcWrite(IN1CHANNEL, speed);
  ledcWrite(IN2CHANNEL, 0);
}
void goDown(int fast = 0){
  int speed;
  if( fast == 1) {
    speed = FAST_SPEED;
  }
  else{
    speed = SLOW_SPEED;
  }
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, speed);
}

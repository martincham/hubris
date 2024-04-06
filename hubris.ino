#include <Arduino.h>

#define IN2 18      // white
#define IN1 17      // gray
#define TOP 6     // purple
#define BOTTOM 5  // red
#define ENCA 1    // white
#define ENCB 3    // yellow
#define IN1CHANNEL 1
#define IN2CHANNEL 2

#define SLEEP_TIME 60 // seconds
#define FAST_SPEED 180
#define SLOW_SPEED 20

#include <PWMOutESP32.h> //i dont know why i still need this but it breaks without

PWMOutESP32 pwm;

#include "RTClib.h"  // I don't know why I need this anymore, but it breaks without it
RTC_DS3231 rtc;

#include <Adafruit_I2CDevice.h>
#include "NTP.h"


int botSwitch = 1;  // 0 when triggered
int topSwitch = 1;  // 0 when triggered

int direction = 0;  //1 is up, -1 down
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
void goUp(int slow =  0);
void goDown(int slow = 0);


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
  Serial.println("Setup");


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

  // awake from sleep
  if (direction == 0 && topSwitch == 0){
    direction = -1; 
    Serial.println("Awoke, going down.");
  }
  else if (direction == 0 && botSwitch == 0){
    direction == 1;
    Serial.println("Awoke, going up.");
  }

  if (topSwitch == 0 && direction == 1) {
    // set down
    direction = -1;
    stopMotor();
    Serial.println("hit top");
    motorPosition = 0;
    prevPosition = 0;
    delay(2000);
    /*
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
    Serial.println("Entering deep sleep mode...");
    esp_deep_sleep_start();  // Start the deep sleep mode
    */
  }
  else if (botSwitch == 0 && direction == -1) {
    //set up
    direction = 1;
    stopMotor();
    Serial.println("hit bottom");
    delay(2000);
    /*
    esp_sleep_enable_timer_wakeup(SLEEP_TIME * uS_TO_S_FACTOR);
    Serial.println("Entering deep sleep mode...");
    esp_deep_sleep_start();  // Start the deep sleep mode
    */
  }

  if (direction == 1) {
    // go up
    goUp();
  } else if (direction == -1) {
    //go down
    goDown();
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

void stopMotor(){
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, 0);
}
void goUp(int slow){
  int speed;
  if( slow == 1) {
    speed = SLOW_SPEED;
  }
  else{
    speed = FAST_SPEED;
  }
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, speed);
}
void goDown(int slow){
  int speed;
  if( slow == 1) {
    speed = SLOW_SPEED;
  }
  else{
    speed = FAST_SPEED;
  }
  ledcWrite(IN1CHANNEL, speed);
  ledcWrite(IN2CHANNEL, 0);
}

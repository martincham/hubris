#include <Arduino.h>
#include "WiFi.h"
#include <esp_now.h>

#define IN2 18   // white
#define IN1 17   // gray
#define TOP 6    // purple
#define BOTTOM 5 // red
#define ENCA 1   // white
#define ENCB 3   // yellow
#define IN1CHANNEL 1
#define IN2CHANNEL 2

#define SLEEP_TIME 60
#define FAST_SPEED 140
#define SLOW_SPEED 100

int sleepTime = SLEEP_TIME; // seconds
int fastSpeed = FAST_SPEED; // PMW out of 255
int slowSpeed = SLOW_SPEED; // PMW out of 255

#include <PWMOutESP32.h>
PWMOutESP32 pwm;

#include "RTClib.h"
RTC_DS3231 rtc;

#include <Adafruit_I2CDevice.h>
#include "NTP.h"

int botSwitch = 1; // 0 when triggered
int topSwitch = 1; // 0 when triggered

int direction = 0; // 1 is up, -1 down, 0 not moving
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
void goToDeepSleep(int sleepDuration);
int calculateSleepDuration();

// ESP NOW SENDING STUFF - yellow pedestal - master
uint8_t broadcastAddress1[] = {0x7C, 0xDF, 0xA1, 0xF9, 0x06, 0x14}; // red
uint8_t broadcastAddress2[] = {0x7C, 0xDF, 0xA1, 0xF8, 0xFC, 0x40}; // blue
uint8_t broadcastAddress3[] = {0x7C, 0xDF, 0xA1, 0xF8, 0xFB, 0xE4}; // green
esp_now_peer_info_t peerInfo;
typedef struct struct_message
{
  int a; // direction
  int b; // sleep time
  int c; // fast speed
  int d; // slow speed
} struct_message;

struct_message espNowMessage;

// Callback function called when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  char macStr[18];
  Serial.print("Packet to: ");
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" send status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void pingAllPedestals(int direction)
{
  espNowMessage.a = direction;
  espNowMessage.b = sleepTime;
  espNowMessage.c = fastSpeed;
  espNowMessage.d = slowSpeed;
  sendPing(espNowMessage);
}

void sendPing(struct_message espNowMessage)
{

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(0, (uint8_t *)&espNowMessage, sizeof(espNowMessage));

  if (result == ESP_OK)
  {
    Serial.println("Sending confirmed");
  }
  else
  {
    Serial.println("Sending error");
  }
}

void setup()
{
  Serial.begin(9600);

  pinMode(TOP, INPUT_PULLUP); // limit switches
  pinMode(BOTTOM, INPUT_PULLUP);
  attachInterrupt(ENCA, doEncoderA, CHANGE);
  attachInterrupt(ENCB, doEncoderB, CHANGE);

  pinMode(IN2, OUTPUT); //  motor PWMs
  pinMode(IN1, OUTPUT);
  ledcAttachPin(IN1, IN1CHANNEL);
  ledcSetup(IN1CHANNEL, frequency, resolution);
  ledcAttachPin(IN2, IN2CHANNEL);
  ledcSetup(IN2CHANNEL, frequency, resolution);

  // initializing the rtc
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC!");
    Serial.flush();
    while (1)
      delay(10);
  }
  // Long sleep when gallery is closed. Run here to not setup wifi when unneeded
  int longSleep = calculateSleepDuration();
  if (longSleep != 0)
  {
    goToDeepSleep(longSleep);
  }

  // Initilize ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // register peer
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  // register first peer

  memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
  }
  // register second peer
  memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
  }
  /// register third peer
  memcpy(peerInfo.peer_addr, broadcastAddress3, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
  }
}

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    insideLoop();
  }
}

void insideLoop()
{
  botSwitch = digitalRead(BOTTOM);
  topSwitch = digitalRead(TOP);

  printTime();
  Serial.print(motorPosition);
  Serial.print(", ");
  diffPosition = abs(motorPosition - prevPosition); // calc change over time and make it always position with abs
  prevPosition = motorPosition;
  Serial.print(diffPosition);
  Serial.print(", ");

  if (direction == 0 && topSwitch == 0)
  {
    direction = -1;
    Serial.println("Awoke, going down.");
    pingAllPedestals(direction);
  }
  if (direction == 0 && topSwitch == 1)
  {
    direction = 1;
    Serial.println("Awoke, going up.");
    pingAllPedestals(direction);
  }

  if (topSwitch == 0 && direction == 1)
  {
    // set down
    direction = -1;
    stopMotor();
    Serial.println("hit top");
    motorPosition = 0;
    prevPosition = 0;
    goToDeepSleep(sleepTime);
  }
  else if (botSwitch == 0 && direction == -1)
  {
    // set up
    direction = 1;
    stopMotor();
    Serial.println("hit bottom");
    goToDeepSleep(sleepTime);
  }

  if (direction == 1)
  {
    // go up
    goUp(1); // SLOWLY
  }
  else if (direction == -1)
  {
    // go down
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

void IRAM_ATTR doEncoderA()
{

  // look for a low-to-high on channel A
  if (digitalRead(ENCA) == HIGH)
  {
    // check channel B to see which way encoder is turning
    if (digitalRead(ENCB) == LOW)
    {
      motorPosition = motorPosition + 1; // CW
    }
    else
    {
      motorPosition = motorPosition - 1; // CCW
    }
  }
  else // must be a high-to-low edge on channel A
  {
    // check channel B to see which way encoder is turning
    if (digitalRead(ENCB) == HIGH)
    {
      motorPosition = motorPosition + 1; // CW
    }
    else
    {
      motorPosition = motorPosition - 1; // CCW
    }
  }
}

void IRAM_ATTR doEncoderB()
{

  // look for a low-to-high on channel B
  if (digitalRead(ENCB) == HIGH)
  {
    // check channel A to see which way encoder is turning
    if (digitalRead(ENCA) == HIGH)
    {
      motorPosition = motorPosition + 1; // CW
    }
    else
    {
      motorPosition = motorPosition - 1; // CCW
    }
  }
  // Look for a high-to-low on channel B
  else
  {
    // check channel B to see which way encoder is turning
    if (digitalRead(ENCA) == LOW)
    {
      motorPosition = motorPosition + 1; // CW
    }
    else
    {
      motorPosition = motorPosition - 1; // CCW
    }
  }
}

void printTime()
{
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

// sleepDuration = minutes
void goToDeepSleep(int sleepDuration)
{
  esp_sleep_enable_timer_wakeup(sleepDuration * uS_TO_S_FACTOR);
  Serial.println("Entering deep sleep mode...");
  esp_deep_sleep_start();
}

// Returns minutes to sleep, or 0 if shouldn't sleep
int calculateSleepDuration()
{
  DateTime now = rtc.now();
  int dayOfTheWeek = now.dayOfTheWeek();
  int hour = now.hour();
  int minute = now.minute();

  // SLEEP SCHEDULE // * 60 seconds for minutes

  if (dayOfTheWeek >= 3)
  { // Wednesday through Saturday
    if (hour >= 12 && hour <= 18)
    { // Time to stay awake
      return 0;
    }
    if (hour <= 8 || hour >= 18)
    {
      return 180 * 60; // three hours
    }
    if (hour < 12)
    { // sleep until 12
      int hourUntil12 = 11 - hour;
      int minuteUntilHour = 59 - minute;
      return ((hourUntil12 * 60) + minuteUntilHour) * 60;
    }
  }
  else
  {                  // Sunday through Tuesday
    return 180 * 60; // three hours
  }
  return 0;
}

// MOTOR FUNCTIONS

void stopMotor()
{
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, 0);
}
void goUp(int slow)
{
  int speed;
  if (slow == 1)
  {
    speed = slowSpeed;
  }
  else
  {
    speed = fastSpeed;
  }
  ledcWrite(IN1CHANNEL, 0);
  ledcWrite(IN2CHANNEL, speed);
}
void goDown(int slow)
{
  int speed;
  if (slow == 1)
  {
    speed = slowSpeed;
  }
  else
  {
    speed = fastSpeed;
  }
  ledcWrite(IN1CHANNEL, speed);
  ledcWrite(IN2CHANNEL, 0);
}

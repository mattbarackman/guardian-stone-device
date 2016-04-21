
#include "TinyGPS.h"
#include "AssetTracker.h"
#include "math.h"

/*BEGIN SWITCH DECLARATIONS */
const int switchPin = D3;
const int RED_PIN = D0;
const int GREEN_PIN = D1;
const int BLUE_PIN = D2;
const int GREEN = 255;
const int YELLOW = 511;
const int RED = 0;
const int SITTING = 0;
const int TRIGGERING = 1;
const int ALERTING = 2;

int buttonState = 0;
long fallTime;
int prevStatus = SITTING;
int on = 1;
int remPower = 100;
int gpsIsOn = 0;

/*END SWITCH DECLARATIONS*/

/*BEGIN GPS DECLARATIONS*/

// Set whether you want the device to publish data to the internet by default here.
// 1 will Particle.publish AND Serial.print, 0 will just Serial.print
// Extremely useful for saving data while developing close enough to have a cable plugged in.
// You can also change this remotely using the Particle.function "tmode" defined in setup()
int transmittingData = 1;

// Used to keep track of the last time we published data
long lastPublish;

// How many minutes between publishes? 10+ recommended for long-time continuous publishing!
int delayMinutes = 2;

char lastLoc[64];
float prevLat = 0.0;
float prevLon = 0.0;

AssetTracker t = AssetTracker();

/* This sample code demonstrates the normal use of a TinyGPS object.
   It requires the use of SoftwareSerial, and assumes that you have a
   4800-baud serial GPS device hooked up on pins 4(rx) and 3(tx).
*/

TinyGPS tgps;

// A FuelGauge named 'fuel' for checking on the battery state
FuelGauge fuel;

/*END GPS DECLARATIONS*/

void setup() {
  // Opens up a Serial port so you can listen over USB
  Serial.begin(9600);
  Particle.variable("O", on);
  Particle.variable("P", remPower);
  Particle.function("turnon", turnOn);
  Particle.function("turnoff", turnOff);
  switchSetup();
  gpsSetup();
}

void gpsSetup() {
  t.begin();
  t.gpsOn();
  gpsIsOn = 1;
  Particle.function("tmode", transmitMode);
  Particle.variable("L", lastLoc);
  Particle.variable("T", lastPublish);
}

int turnOn(String command) {
  t.gpsOn();
  gpsIsOn = 1;
  on = 1;
  fallTime = Time.now();
  prevStatus = SITTING;
  return 1;
}

int turnOff(String command) {
  t.gpsOff();
  gpsIsOn = 0;
  on = 0;
  fallTime = Time.now();
  analogWrite(RED_PIN, 0);
  analogWrite(BLUE_PIN, 0);
  analogWrite(GREEN_PIN, 0);
  return 1;
}

void switchSetup() {
  fallTime = Time.now();
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(switchPin, INPUT);
}

void loop() {
  if (on == 1) {
    if ((millis() % 1000L) <= 10L){
      Serial.println("tick");
    }
    switchLoop();
    tinyGPSLoop();
    batteryLoop();
  }
}

void switchLoop() {
  switch (status()) {
    case SITTING:
      showRGB(GREEN);
      prevStatus = SITTING;
      break;

    case TRIGGERING:
      showRGB(YELLOW);
      prevStatus = TRIGGERING;
      break;

    case ALERTING:
      showRGB(RED);
      prevStatus = ALERTING;
      break;
  }
}

bool delayPassed() {
  /*return true;*/
  return Time.now()-lastPublish > delayMinutes*60;
}

void tinyGPSLoop() {

  bool isValidGPS = false;
  char gpsLoc[64];

  if ((millis() % 1000L) <= 10L){
    if (gpsIsOn){
      Serial.println("gpsIsOn");
    }
  }

  if (delayPassed()){

    /*if ((millis() % 1000L) <= 10L){
      Serial.println("gps_loop");
    }*/

    if (gpsIsOn == 0) {
      /*Serial.println("turn gps on");*/
      t.gpsOn();
      /*Serial.println("turn gps on done");*/
      gpsIsOn = 1;
    }

    char c = t.checkGPS();
    int b = tgps.encode(c);
    if (b){
      isValidGPS = true;
    }

    if (isValidGPS){
      float lat, lon, dis;
      unsigned long age;

      tgps.f_get_position(&lat, &lon, &age);

      sprintf(gpsLoc, "%.6f,%.6f", (lat == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : lat), (lon == TinyGPS::GPS_INVALID_F_ANGLE ? 0.0 : lon));

      if(gpsLoc != "0.0,0.0"){
        // update cloud variable();
        lastPublish = Time.now();
        dis = distance(prevLat, prevLon, lat, lon);
        /*Serial.println(dis);*/
        if (dis >= 100){
          /*Serial.println("setting new coords");*/
          prevLat = lat;
          prevLon = lon;
          strncpy(lastLoc, gpsLoc, 64);
        } else {
          /*Serial.println("too close");*/
        }
        // update cloud variable();
        /*Serial.println("turn gps off");*/
        t.gpsOff();
        /*Serial.println("turn gps off done");*/
        gpsIsOn = 0;
      }
    }

  }
}

bool onChair() {
  if (digitalRead(switchPin) == LOW) {
    return true;
  } else {
    return false;
  }
}

long elapsedTime(long ot) {
  return Time.now() - ot;
}

int status() {
  if (onChair()){
    return SITTING;
  }

  if (prevStatus == SITTING) {
    fallTime = Time.now();
  }

  long et = elapsedTime(fallTime);

  if (et <= 5) {
    return TRIGGERING;
  } else if (et > 5){
    return ALERTING;
  }

}

void showRGB(int color)
{
  int redIntensity;
  int greenIntensity;
  int blueIntensity;

  // Here we'll use an "if / else" statement to determine which
  // of the three (R,G,B) zones x falls into. Each of these zones
  // spans 255 because analogWrite() wants a number from 0 to 255.

  // In each of these zones, we'll calculate the brightness
  // for each of the red, green, and blue LEDs within the RGB LED.

  if (color <= 255)          // zone 1
  {
    redIntensity = 255 - color;    // red goes from on to off
    greenIntensity = color;        // green goes from off to on
    blueIntensity = 0;             // blue is always off
  }
  else if (color <= 511)     // zone 2
  {
    redIntensity = 0;                     // red is always off
    greenIntensity = 255 - (color - 256); // green on to off
    blueIntensity = (color - 256);        // blue off to on
  }
  else // color >= 512       // zone 3
  {
    redIntensity = (color - 512);         // red off to on
    greenIntensity = 0;                   // green is always off
    blueIntensity = 255 - (color - 512);  // blue on to off
  }

  // Now that the brightness values have been set, command the LED
  // to those values

  analogWrite(RED_PIN, redIntensity);
  analogWrite(BLUE_PIN, blueIntensity);
  analogWrite(GREEN_PIN, greenIntensity);
}

int transmitMode(String command){
    transmittingData = atoi(command);
    return 1;
}

void batteryLoop(){
  int locPower = trunc((fuel.getSoC()) / 5 ) * 5;
  if (batteryChanged(locPower)) {
    remPower = locPower;
  }
}

bool batteryChanged(int lp){
  return lp != remPower;
}

float distance(float lat1, float lon1, float lat2, float lon2){
  float dlon = lon2 - lon1;
  float dlat = lat2 - lat1;
  float a = pow((sin(dlat/2)), 2) + cos(lat1) * cos(lat2) * pow((sin(dlon/2)), 2);
  float c = 2.0 * atan2(sqrt(a), sqrt(1-a));
  return 6371000.0 * c;
}

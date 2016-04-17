
#include "AssetTracker.h"



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
long offTime;
int prevStatus = SITTING;
int on = 1;
int power = 10;

/*END SWITCH DECLARATIONS*/

/*BEGIN GPS DECLARATIONS*/

// Set whether you want the device to publish data to the internet by default here.
// 1 will Particle.publish AND Serial.print, 0 will just Serial.print
// Extremely useful for saving data while developing close enough to have a cable plugged in.
// You can also change this remotely using the Particle.function "tmode" defined in setup()
int transmittingData = 1;

// Used to keep track of the last time we published data
long lastPublish = 0;

// How many minutes between publishes? 10+ recommended for long-time continuous publishing!
int delayMinutes = 10;

// Creating an AssetTracker named 't' for us to reference
AssetTracker t = AssetTracker();

// A FuelGauge named 'fuel' for checking on the battery state
FuelGauge fuel;

/*END GPS DECLARATIONS*/

void setup() {
  // Opens up a Serial port so you can listen over USB
  Serial.begin(9600);
  Particle.variable("on", on);
  Particle.variable("power", on);
  on = 1;
  Particle.function("turnon", turnOn);
  Particle.function("turnoff", turnOff);
  switchSetup();
  gpsSetup();
}

void gpsSetup() {
  // These three functions are useful for remote diagnostics. Read more below.
  Particle.function("tmode", transmitMode);
  Particle.function("gps", gpsPublish);
  // Sets up all the necessary AssetTracker bits
  t.begin();
  // Enable the GPS module. Defaults to off to save power.
  // Takes 1.5s or so because of delays.
  t.gpsOn();
}

int turnOn(String command) {
  t.gpsOn();
  on = 1;
  offTime = Time.now();
  prevStatus = SITTING;
  return 1;
}

int turnOff(String command) {
  t.gpsOff();
  on = 0;
  offTime = Time.now();
  analogWrite(RED_PIN, 0);
  analogWrite(BLUE_PIN, 0);
  analogWrite(GREEN_PIN, 0);
  return 1;
}

void switchSetup() {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(switchPin, INPUT);
  Particle.function("batt", batteryStatus);
}

void loop() {
  if (on == 1) {
    switchLoop();
    gpsLoop();
    /*batteryLoop();*/
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
  return millis()-lastPublish > delayMinutes*60*1000;
}

void gpsLoop() {
    // You'll need to run this every loop to capture the GPS output
  t.updateGPS();

  // if the current time - the last time we published is greater than your set delay...
  if(delayPassed()){
      // Remember when we published
      lastPublish = millis();

      //String pubAccel = String::format("%d,%d,%d",t.readX(),t.readY(),t.readZ());
      //Serial.println(pubAccel);
      //Particle.publish("A", pubAccel, 60, PRIVATE);

      // Dumps the full NMEA sentence to serial in case you're curious
      Serial.println(t.preNMEA());

      // GPS requires a "fix" on the satellites to give good data,
      // so we should only publish data if there's a fix
      if(t.gpsFix()){
          // Only publish if we're in transmittingData mode 1;
          if(transmittingData){
              // Short publish names save data!
              Particle.publish("G", t.readLatLon(), 60, PRIVATE);
          }
          // but always report the data over serial for local development
          Serial.println(t.readLatLon());
      }
  }
}

bool onChair() {
  Serial.println(digitalRead(switchPin));
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
    offTime = Time.now();
  }

  long et = elapsedTime(offTime);

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

// Actively ask for a GPS reading if you're impatient. Only publishes if there's
// a GPS fix, otherwise returns '0'
int gpsPublish(String command){
    if(t.gpsFix()){
        Particle.publish("G", t.readLatLon(), 60, PRIVATE);

        // uncomment next line if you want a manual publish to reset delay counter
        // lastPublish = millis();
        return 1;
    }
    else { return 0; }
}

void batteryLoop(){
  if (batteryChanged()) {
    power = fuel.getSoC() / 10;
    publishBatteryStatus();
  }
}

bool batteryChanged(){
  power != fuel.getSoC() / 10
}

// Lets you remotely check the battery status by calling the function "batt"
// Triggers a publish with the info (so subscribe or watch the dashboard)
// and also returns a '1' if there's >10% battery left and a '0' if below
int batteryStatus(String command){
    publishBatteryStatus();
    return fuel.getSoC() / 10;
}

void publishBatteryStatus(){
  Particle.publish("power", power, 16777215);
}

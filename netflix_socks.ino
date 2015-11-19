
// ======================== LIBS ==============================
#include <avr/power.h>
#include <avr/sleep.h>
#include <Adafruit_NeoPixel.h>

//for debugging lcd display
#include <LiquidCrystal.h>

// ======================== PINS ==============================
//for the indicator light is a neopixel
#define INDICATOR_PIN 8
#define SOFT_SWITCH_PIN 3

//accelerometers
#define x A3
#define y A4
#define z A5

//ir to stop the show (stop button on our Sony NRTV remote)
#define IR_PIN     4
#define NUM_PULSES 96
int pulse_widths[NUM_PULSES][2] = {
  {37852, 2380},  {560, 600},  {560, 600},  {560, 1180},  {560, 1200},  {560, 1160},  {580, 1180},
  {580, 1180},  {560, 580},  {580, 1180},  {560, 600},  {560, 1180},  {580, 1160},
  {580, 580},  {580, 600},  {560, 600},  {19380, 2380},  {560, 600},  {560, 600},
  {560, 1180},  {580, 1180},  {560, 1180},  {580, 1180},  {560, 1180},  {560, 600},
  {560, 1180},  {580, 580},  {580, 1180},  {560, 1180},  {580, 580},  {560, 600},
  {560, 600},  {19380, 2400},  {560, 600},  {560, 600},  {560, 1180},  {560, 1180},
  {560, 1180},  {580, 1180},  {560, 1180},  {580, 580},  {580, 1180},  {560, 580},
  {580, 1180},  {560, 1180},  {580, 580},  {580, 580},  {580, 580},  {19440, 2380},
  {560, 600},  {560, 600},  {560, 1180},  {580, 1160},  {580, 1180},  {560, 1200},
  {560, 1180},  {560, 600},  {560, 1180},  {560, 600},  {560, 1200},  {560, 1160},
  {580, 600},  {560, 600},  {560, 580},  {19440, 2380},  {580, 580},  {580, 580},
  {560, 1180},  {580, 1180},  {560, 1180},  {580, 1180},  {560, 1180},  {560, 600},
  {560, 1180},  {580, 580},  {580, 1180},  {560, 1180},  {560, 600},  {560, 600},
  {560, 600},  {19420, 2380},  {580, 600},  {560, 600},  {560, 1180},  {560, 1180},
  {580, 1160},  {580, 1180},  {560, 1180},  {580, 580},  {580, 1180},  {560, 600},
  {560, 1180},  {560, 1180},  {580, 580},  {560, 600},  {560, 600}
};


// ======================== VARS ==============================

Adafruit_NeoPixel indicator = Adafruit_NeoPixel(1, INDICATOR_PIN);
uint8_t color = 255;

//LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
LiquidCrystal lcd(5, 6 , 10, 11, 12, 13);
volatile bool cpuSleepFlag = true;

int userSleepState = 0; //0 awake, 1 falling asleep, 2 asleep

unsigned long nextReadTime, windowTime, irDebugTime, cpuAwoken;
int readDelay = 50; //read accelerometer every 50 ms
int windowDelay = 2000; //compute displacement every window
int irDebugDelay = 2000;
bool newWindow = true;


//accelerometer values
int pxVal = 0;
int pyVal = 0;
int pzVal = 0;
int movementDisplacement = 0;
int consecutivePossibleSleeps = 0;

int threshold = 500; //amount of movement that sleep is less than
int thresholdTime = 1; // in minutes

bool state = false;

// ======================== SETUP ==============================
void setup() {
  pinMode(SOFT_SWITCH_PIN, INPUT);
  pinMode(IR_PIN, OUTPUT);
  digitalWrite(IR_PIN, LOW);
  //setup the timers
  nextReadTime = millis();
  windowTime = nextReadTime;
  //indicator light
  indicator.begin();
  indicator.setBrightness(100); 
  //debug lcd
  lcd.begin(16, 2);
  //start asleep
  cpuSleepNow();
}

// ======================== LOOP ==============================

void loop() {
  indicatorHandler();
  softSwitchHandler();

  //if a window time is up, make it a new window for a sliding window average
  if (millis() - windowTime > windowDelay) {
    windowTime = millis();
    newWindow = true;
  }
  
  //if we've reached the readDelay, read accelerometer
  if (millis() - nextReadTime > readDelay) {
    nextReadTime = millis();
//    accelerometerHandler();
  }
  
  //for debug, just blast IR every X seconds
  if (millis() - irDebugTime > irDebugDelay) {
//    IR_transmit_pwr();
  }
  
  //a little delay
  delay(100);
  digitalWrite(IR_PIN, HIGH);
  delay(100);
      IR_transmit_pwr();
  digitalWrite(IR_PIN, LOW);
    delay(1000);

}
void softSwitchHandler(){
  if(digitalRead(SOFT_SWITCH_PIN) == LOW && millis()-cpuAwoken > 1000){
    //its held down. 
    cpuSleepNow();
  }else{
  
  }
}

// ======================== ACCELEROMETER ==============================
void accelerometerHandler() {
  //read the accelerometer
  int xVal = analogRead(x);
  int yVal = analogRead(y);
  int zVal = analogRead(z);
  
  //if its a new iwn
  if (newWindow) {
    //just set the values to what they currently are
    lcd.clear();
    if (userSleepState == 2) {
      lcd.setCursor(0, 0);
      lcd.print("SLEEP");
    }
    //    Serial.print(movementDisplacement);
    //    Serial.println(" movement in last second");
    lcd.setCursor(0, 1);
    lcd.print(movementDisplacement);
    if (movementDisplacement > 255) {
     // analogWrite(9, 255);
    } else {
     // analogWrite(9, movementDisplacement);
    }
    if (movementDisplacement < threshold) {
      userSleepState = 1;
    } else {
      userSleepState = 0;
    }

    if (userSleepState == 1) {
      consecutivePossibleSleeps++;
      if (consecutivePossibleSleeps > ((1000 / windowDelay) * 60 * thresholdTime)) {
        detectedSleep();
      } //120 per minute
    } else {
      consecutivePossibleSleeps = 0;
    }


    pxVal = xVal;
    pyVal = yVal;
    pzVal = zVal;
    movementDisplacement = 0;
    newWindow = false;
  } else {
    //compute magnitude changes
    movementDisplacement += abs(xVal - pxVal) + abs(yVal - pyVal) + abs(zVal - pzVal);
  }
}


// ======================== INDICATOR ==============================

void indicatorHandler(){
  //pulse
  color--;
  if(color < 1){
    color = 255;
  }
  indicator.setPixelColor(0,color,color,color);
  indicator.show();
}

// ======================== CPU SLEEP ==============================

void cpuSleepNow() {  
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here  
    sleep_enable();          // enables the sleep bit in the mcucr register  
    indicator.clear();
    attachInterrupt(digitalPinToInterrupt(3),pinInterrupt, LOW); // use interrupt 0 (pin 2) and run function  
    sleep_mode();            // here the device is actually put to sleep!!  
    // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP  
    sleep_disable();         // first thing after waking from sleep: disable sleep...  
    detachInterrupt(digitalPinToInterrupt(3));      // disables interrupt 0 on pin 2 so the wakeUpNow code will not be executed during normal running time.  
}  


void pinInterrupt()  
{  
//    detachInterrupt(digitalPinToInterrupt(3));  
//    attachInterrupt(digitalPinToInterrupt(3), pinInterrupt, HIGH);  
    cpuSleepFlag = false;
    cpuAwoken = millis();
}  


void detectedSleep() {
  userSleepState = 2;
}

//==
void pulseIR(long microsecs) {
  while (microsecs > 0) {
    // 38 kHz is about 13 microseconds high and 13 microseconds low
    digitalWrite(IR_PIN, HIGH);  // this takes about 3 microseconds to happen
    delayMicroseconds(10);         // hang out for 10 microseconds
    digitalWrite(IR_PIN, LOW);   // this also takes about 3 microseconds
    delayMicroseconds(10);         // hang out for 10 microseconds
    // so 26 microseconds altogether
    microsecs -= 26;
  }
}

void IR_transmit_pwr() {
  for (int i = 0; i < NUM_PULSES; i++) {
    delayMicroseconds(pulse_widths[i][0]);
    pulseIR(pulse_widths[i][1]);
  }
}


  #define DEBUG 2

// ======================== LIBS ==============================
#include <avr/power.h>
#include <avr/sleep.h>
#include <Adafruit_NeoPixel.h>

//for debugging lcd display
#if DEBUG == 1
  #include <LiquidCrystal.h>
#endif
// ======================== PINS ==============================
//for the indicator light its a neopixel
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
#if DEBUG == 1
  LiquidCrystal lcd(5, 6 , 10, 11, 12, 13);
#endif
volatile bool cpuSleepFlag = true;

int userSleepState = 0; //0 awake, 1 falling asleep, 2 asleep
bool userReallyAsleep = false;
unsigned long nextReadTime, windowTime, irDebugTime, cpuAwoken, userReallyAsleepStart;
int readDelay = 50; //read accelerometer every 50 ms
int windowDelay = 2000; //compute displacement every window
int irDebugDelay = 2000;
int userReallyAsleepDelay = 1 * 60 * 1000; // 1 minute
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
#if DEBUG == 1
    lcd.begin(16, 2);
#endif
#if DEBUG == 2
     pinMode(13, OUTPUT);
     digitalWrite(13, HIGH);
#endif
 
  //start asleep
  cpuSleepNow();

}

// ======================== LOOP ==============================

void loop() {
#if DEBUG == 2
     digitalWrite(13, HIGH);
#endif
  indicatorHandler();
  softSwitchHandler();
  sleepHandler();

  //if a window time is up, make it a new window for a sliding window average
  if (millis() - windowTime > windowDelay) {
    windowTime = millis();
    newWindow = true;
  }
  
  //if we've reached the readDelay, read accelerometer
  if (millis() - nextReadTime > readDelay) {
    nextReadTime = millis();
   // accelerometerHandler();
  }
  
  //for debug, just blast IR every X seconds
  if (millis() - irDebugTime > irDebugDelay) {
    irDebugTime = millis();
#if DEBUG == 2
    digitalWrite(13, LOW);
    delay(100);
    digitalWrite(13, HIGH);
#endif
    IR_transmit_pwr();
  }
  
  

}

// ======================== SOFT SWITCH ==============================
void softSwitchHandler(){
  if(digitalRead(SOFT_SWITCH_PIN) == LOW && millis()-cpuAwoken > 1000){
    //its held down. 
    cpuSleepNow();
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
#if DEBUG == 1
      lcd.clear();

    //    Serial.print(movementDisplacement);
    //    Serial.println(" movement in last second");
    lcd.setCursor(0, 1);
    lcd.print(movementDisplacement);
#endif
    if (movementDisplacement < threshold) {
      userSleepState = 1;
    } else {
      userSleepState = 0;
    }

    if (userSleepState == 1) {
      consecutivePossibleSleeps++;
      if (consecutivePossibleSleeps > ((1000 / windowDelay) * 60 * thresholdTime)) {
        userSleepState = 2;
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
// ======================== USER SLEEP ==============================
void sleepHandler(){
  if(userSleepState == 2){
    if(millis() - userReallyAsleepStart > userReallyAsleepDelay){
      //the user is really asleep. fire off IR. 
    }
  }else{
    userReallyAsleepStart = millis();
  }
#if DEBUG == 1  
  if(userSleepState == 2){
    lcd.setCursor(0,0);
    lcd.print("SLEEP");
  }else if(userSleepState == 1){
    lcd.setCursor(0,0);
    lcd.print("MAYBE SLEEP");
  }
#endif
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
    sleep_enable();
    delay(100);
    indicator.clear();
#if DEBUG == 2
    digitalWrite(13, LOW);
#endif
    attachInterrupt(digitalPinToInterrupt(3),pinInterrupt, FALLING);
    sleep_mode();
    // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP  
    sleep_disable();
    detachInterrupt(digitalPinToInterrupt(3));
}  


void pinInterrupt()  
{  
    cpuAwoken = millis();
}  


// ======================== IR ==============================
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

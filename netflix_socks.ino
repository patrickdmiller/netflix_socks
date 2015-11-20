
#define DEBUG 0
#define LCD true
// ======================== LIBS ==============================
#include <avr/power.h>
#include <avr/sleep.h>
#include <Adafruit_NeoPixel.h>

//for debugging lcd display
#if LCD == true
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
#define NUM_PULSES 60
/*
int zpulse_widths[NUM_PULSES][2] = {
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
*/

int pulse_widths[NUM_PULSES][2] = {{4716,2416},{572,628},{568,628},{572,616},{580,1224},{572,1224},{568,620},{580,620},{572,1220},{576,1228},{568,1224},{572,616},{580,1224},{572,620},{576,620},{576,1220},{20308,2420},{572,628},{568,620},{576,624},{572,1220},{576,1220},{576,620},{576,624},{572,1220},{576,1220},{572,1220},{576,624},{572,1220},{576,624},{572,620},{576,1224},{20304,2416},{572,620},{576,620},{576,624},{572,1224},{572,1220},{576,624},{572,628},{568,1224},{568,1224},{572,1224},{572,624},{572,1224},{568,628},{572,620},{576,1216},{20360,2420},{568,620},{576,624},{572,628},{568,1224},{572,1224},{572,624},{568,624},{576,1216},{576,1216},{580,1224},{572,620}};



// ======================== VARS ==============================

Adafruit_NeoPixel indicator = Adafruit_NeoPixel(1, INDICATOR_PIN);
///color for each state.
uint8_t colors[3][3] = {{1, 1, 1}, {1, 0, 1}, {1, 0, 0}}; //color multiplier per state, 1,1,1 is white, 1,0,0, is red, etc.
uint8_t colorState = 255;
uint8_t colorPulseIncrement = -1;

//LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
#if LCD == true
  LiquidCrystal lcd(5, 6 , 10, 11, 12, 13);
#endif
volatile bool cpuSleepFlag = true;

int userSleepState = 0; //0 awake, 1 possible asleep, 2 asleep
bool userReallyAsleep = false;
unsigned long nextReadTime, windowTime, irDebugTime, cpuAwoken, userReallyAsleepStart;
int readDelay = 50; //read accelerometer every 50 ms
int windowDelay = 1500; //compute displacement every window
int irDebugDelay = 2000;
int userReallyAsleepDelay = 1 * 60 * 1000; // 1 minute
int consecutivePossibleSleeps = 0;
int threshold = 50; //amount of movement that sleep is less than. this is a sum magnitude so the lower the window, the lower this should be. This can be converted to a rate by making it a fn of window time.
int thresholdTime = 30; // in seconds

bool newWindow = true;

//accelerometer values
int pxVal = 0;
int pyVal = 0;
int pzVal = 0;
int movementDisplacement = 0;


bool state = false;

// ======================== SETUP ==============================
void setup() {
  analogReference(EXTERNAL); // use AREF for reference voltage
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
#if LCD == true
    lcd.begin(16, 2);
#endif
#if DEBUG == 1
     pinMode(13, OUTPUT);
     digitalWrite(13, HIGH);
#endif
 
  //start asleep
  cpuSleepNow();

}

// ======================== LOOP ==============================

void loop() {
#if DEBUG == 1
  digitalWrite(13, HIGH);
#endif
  indicatorHandler();
  softSwitchHandler();
  sleepHandler();
  irHandler();
  //if a window time is up, make it a new window for a sliding window average
  if (millis() - windowTime > windowDelay) {
    windowTime = millis();
    newWindow = true;
  }
  
  //if we've reached the readDelay, read accelerometer
  if (millis() - nextReadTime > readDelay) {
    nextReadTime = millis();
   accelerometerHandler();
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
#if LCD == true
      lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(movementDisplacement);
#endif
    if (movementDisplacement < threshold) {
      if(userSleepState !=2){
        userSleepState = 1;  
      }
    } else {
      userSleepState = 0;
    }
    
        lcd.print("a");
        lcd.print(userSleepState);

    if (userSleepState >= 1) {
      consecutivePossibleSleeps++;
      lcd.print("b");
      if (consecutivePossibleSleeps > (thresholdTime * ((float)1000/windowDelay))){//((1000 / windowDelay) * 60 * thresholdTime)) {
        userSleepState = 2;
//            lcd.setCursor(0, 1);

              lcd.print(consecutivePossibleSleeps);

      } //120 per minute
    } else {
//      userSleepState = 1;
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
      userReallyAsleep = true;
    }
  }else{
    userReallyAsleepStart = millis();
  }
#if LCD == true  
  if(userReallyAsleep){
    lcd.setCursor(0,0);
    lcd.print("DEF SLEEP!");
  }else if(userSleepState == 2){
    lcd.setCursor(0,0);
    lcd.print("SLEEP");
  }else if(userSleepState == 1){
    lcd.setCursor(0,0);
    lcd.print("MAYBE SLEEP");
  }else{
    lcd.setCursor(0,0);
    lcd.print("NOT SLEEP");
  }
#endif
}

// ======================== INDICATOR ==============================

void indicatorHandler(){
  //pulse
  colorState+=colorPulseIncrement;
  if(colorState < 1){
    colorPulseIncrement = 0;
  }else if(colorState >254){
    colorPulseIncrement = 0;
  }
  colorState = 255;
  indicator.setPixelColor(0,colorState * colors[userSleepState][0], colorState * colors[userSleepState][1], colorState * colors[userSleepState][2]);
  indicator.show();
}

// ======================== CPU SLEEP ==============================

void cpuSleepNow() {  
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here  
    sleep_enable();
    delay(100);
//    indicator.clear();
    indicator.setPixelColor(0,0,0,0);
  indicator.show();
#if DEBUG == 1
    digitalWrite(13, LOW);
#endif
    attachInterrupt(digitalPinToInterrupt(3),pinInterrupt, FALLING);
    sleep_mode();
    // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP  
    sleep_disable();
    //reset user state snice turning back on
    //probably should reset other stuff here too
    userSleepState = 0;
    detachInterrupt(digitalPinToInterrupt(3));
}  


void pinInterrupt()  
{  
    cpuAwoken = millis();
}  


// ======================== IR ==============================
void irHandler(){
  //for debug, just blast IR every X seconds
  if (millis() - irDebugTime > irDebugDelay) {
    irDebugTime = millis();
#if DEBUG == 1
    digitalWrite(13, LOW);
    delay(100);
    digitalWrite(13, HIGH);
#endif
    IR_transmit_pwr();
  }
  if(userReallyAsleep){
//    IR_transmit_pwr();
  }
}

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

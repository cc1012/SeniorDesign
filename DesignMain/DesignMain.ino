#include <TimerOne.h>
#include <LiquidCrystal.h>
#include <inttypes.h>

const int led = 13;
const int navBtn = 10;
const int selBtn = 11;

// Wheel Encoder variables
const int WEA = 2;  // int0
const int WEB = 3;  // int1
volatile int WECounts = 0;
volatile int WEError = 0;
volatile byte last_aVal, last_bVal;

// initialize LC library with the pins to be used
// Pin2 = rs
// Pin3 = enable
// Pin4 = d4
// Pin5 = d5
// Pin6 = d6
// Pin7 = d7
//LiquidCrystal lcd(2,3,4,5,6,7);
LiquidCrystal lcd(4,5,6,7,8,9);

// Define State Machine
const byte waitForInput = 1;  // Wait on user input
const byte navBtnPressed = 2;   // Navigation button pressed
const byte selBtnPressed = 3;   // Select button pressed

void configPins()
{
  // setup led
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  
  // setup buttons
  pinMode(navBtn, INPUT_PULLUP);
  pinMode(selBtn, INPUT_PULLUP);
  
  // Wheel Encoder 
  noInterrupts();
  attachInterrupt(0, WEHandler, CHANGE);
  attachInterrupt(1, WEHandler, CHANGE);
  last_aVal = digitalRead(WEA);
  last_bVal = digitalRead(WEB);
  interrupts();
}

void WEHandler()
{
  // Read current values of the outputs
  byte aVal = digitalRead(WEA);
  byte bVal = digitalRead(WEB);
  
  // Compare to last values (Quadrature Algorithm found in PololuWheelEncoders.cpp)
  byte plus = aVal ^ last_bVal;
  byte minus = bVal ^ last_aVal;
  
  // Update counter appropriately
  if (plus) WECounts++;
  if (minus) WECounts--;
  
  // if both outputs change at same time, that's an error
  if (aVal != last_aVal && bVal != last_bVal)
  {
    WEError = 1;
  }
    
  // Update "last" values
  last_aVal = aVal;
  last_bVal = bVal;
}

void splashScreen()
{
  lcd.setCursor(0,0);
  lcd.print("     Automatic      ");
  lcd.setCursor(0,1);
  lcd.print("   Weightlifting    ");
  lcd.setCursor(0,2);
  lcd.print("      Spotter       ");
  lcd.setCursor(0,3);
  lcd.print("                    ");
  delay(3000);
}

void setupMenu()
{
  lcd.setCursor(0,0);
  lcd.print("Select an Option:   ");
  lcd.setCursor(0,1);
  lcd.print(">Lift               ");
  lcd.setCursor(0,2);
  lcd.print(" Calibrate          ");
  lcd.setCursor(0,3);
  lcd.print(" Statistics         ");
}

void comingSoon()
{
  lcd.setCursor(0,0);
  lcd.print("                    ");
  lcd.setCursor(0,1);
  lcd.print("    Coming Soon!    ");
  lcd.setCursor(0,2);
  lcd.print("                    ");
  lcd.setCursor(0,3);
  lcd.print("                    ");
}

void configTimer()
{
  Timer1.initialize(500000);  // initialize timer for 500 ms
  Timer1.attachInterrupt(timerISR);  // attach interrupt to function
}

void timerISR()
{
  digitalWrite(led, digitalRead(led) ^ 1);
}

void setup()
{
  configPins();
  configTimer();
  
  Serial.begin(9600);
  
  // set up the LCD's number of columns and rows:
  lcd.begin(20,4);
  
  // Print a message to the LCD.
  splashScreen();
  setupMenu();
}

void loop()
{
  static uint8_t menuRow = 1;
  static byte myState = waitForInput;
  
  switch (myState)
  {
    case waitForInput:
      if (digitalRead(navBtn) == LOW) myState = navBtnPressed;
      else if (digitalRead(selBtn) == LOW) myState = selBtnPressed;
      break;
      
    case navBtnPressed:
      lcd.setCursor(0,menuRow);
      lcd.print(" ");
      menuRow++;
      if (menuRow > 3) menuRow = 1;
      lcd.setCursor(0,menuRow);
      lcd.print(">");
      myState = waitForInput;
      break;
      
    case selBtnPressed:
      comingSoon();
      delay(3000);
      setupMenu();
      myState = waitForInput;
      break;
      
    default:
      // blink LED to indicate error
      digitalWrite(led, !digitalRead(led));
      break;
  }
  
  noInterrupts();
  int tmpCnt = WECounts;
  interrupts();
  
  Serial.print("WE Counts: ");
  Serial.print(tmpCnt);
  Serial.println();
  Serial.println();  
  
  delay(100);
}

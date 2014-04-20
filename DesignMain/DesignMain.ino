#include <TimerOne.h>
#include <LiquidCrystal.h>
#include <inttypes.h>

// General IO pins
const int led = 13;
const int navBtn = 10;
const int selBtn = 11;
const int buzzer = 28;

// Motor Control
const int MCDirA = 22;
const int MCDirB = 23;
const int MCFullSpeed = 24;
const int MCVarSpeed = 25;
const int MCVSA = 26;
const int MCVSB = 27;

// Pressure Sensor
const int pressureSensor = 0;
int weight = 0;

// foot pedal
const int footPedal = 1;

// Wheel Encoder variables
const int WEA = 2;  // int0
const int WEB = 3;  // int1
volatile int WECounts = 0;
volatile int WEError = 0;
volatile byte last_aVal, last_bVal;
int helpLevel = 0;

// Timer vals
volatile int lastCount = 0;
volatile boolean upwards = false;
volatile boolean downwards = false;
volatile int stall = 0;
volatile int countDiff = 0;
const int speedThreshold = 50;

// initialize LC library with the pins to be used
// Pin2 = rs
// Pin3 = enable
// Pin4 = d4
// Pin5 = d5
// Pin6 = d6
// Pin7 = d7
//LiquidCrystal lcd(2,3,4,5,6,7);
LiquidCrystal lcd(4,5,6,7,8,9);

// Define Main State Machine
const byte waitForInput = 1;  // Wait on user input
const byte navBtnPressed = 2;   // Navigation button pressed
const byte selBtnPressed = 3;   // Select button pressed
boolean returnToMainMenu = false;

// defines for lift subroutine
const byte barInRack = 1;
const byte downwardState = 2;
const byte upwardState = 3;

// defines for calibrate subroutine
int topVal = 100;
int bottomVal = -100;

// flags
boolean calibrateFlag = false;
boolean liftingFlag = false;

void configPins()
{
  // foot pedal
  pinMode(footPedal, INPUT_PULLUP);
  
  // buzzer
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);
  
  // Motor Control
  pinMode(MCDirA, OUTPUT);
  digitalWrite(MCDirA, LOW);
  pinMode(MCDirB, OUTPUT);
  digitalWrite(MCDirB, LOW);
  pinMode(MCFullSpeed, OUTPUT);
  digitalWrite(MCFullSpeed, LOW);
  pinMode(MCVarSpeed, OUTPUT);
  digitalWrite(MCVarSpeed, LOW);
  pinMode(MCVSA, OUTPUT);
  digitalWrite(MCVSA, LOW);
  pinMode(MCVSB, OUTPUT);
  digitalWrite(MCVSB, LOW);
  
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
    WEError ++;
  }
    
  // Update "last" values
  last_aVal = aVal;
  last_bVal = bVal;
}

void timerISR()
{
  digitalWrite(led, digitalRead(led) ^ 1);
  countDiff = WECounts - lastCount;
  
  if (countDiff > 0)
  {
    upwards = true;
    downwards = false;
    stall = 0;
  }
  else if (countDiff < 0)
  {
    upwards = false;
    downwards = true;
    stall = 0;
  }
  else
  {
    upwards = downwards = false;
    stall++;
  }
  
  lastCount = WECounts;
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
  Timer1.initialize(500000);  // initialize timer for 500 ms (don't do math in function call
  Timer1.attachInterrupt(timerISR);  // attach interrupt to function
}

void MCSpoolOut()
{
  digitalWrite(MCDirA, HIGH);
  digitalWrite(MCDirB, LOW);
}

void MCReelIn()
{
  digitalWrite(MCDirA, LOW);
  digitalWrite(MCDirB, HIGH);
}

void MCShutOff()
{
  digitalWrite(MCDirA, LOW);
  digitalWrite(MCDirB, LOW);
  digitalWrite(MCFullSpeed, LOW);
  digitalWrite(MCVarSpeed, LOW);
  digitalWrite(MCVSA, LOW);
  digitalWrite(MCVSB, LOW);
}

void emergencyLift()
{
  while(WECounts < 0)
  {
    MCReelIn();
    digitalWrite(MCFullSpeed, HIGH);
  }
  digitalWrite(MCDirA, LOW);
  digitalWrite(MCDirB, LOW);
  digitalWrite(MCFullSpeed, LOW);
  helpLevel = 3;
}

void readWeight()
{
  int digVal = analogRead(pressureSensor);
  float analogVal = (float) digVal / 1023.0 * 5.0;
  
  if (analogVal < 1) weight = 0;
  else if (analogVal < 2) weight = 45;
  else if (analogVal < 2.8) weight = 135;
  else if (analogVal < 3.1) weight = 225;
  else if (analogVal < 3.3) weight = 315;
  else if (analogVal < 3.5) weight = 445;
  else weight = 495;
}

void calibrate()
{
  readWeight();
  if ((weight == 45) && (digitalRead(footPedal) == LOW))
  {
    while(analogRead(pressureSensor) > 1);
    
    int sameCount = 0;
    int lastCount = WECounts;
    delay(100);
    while(sameCount < 10)
    {
      if (WECounts == lastCount) sameCount++;
      lastCount = WECounts;
      delay(100);
    }
    topVal = lastCount;
    digitalWrite(buzzer, HIGH);
    delay(500);
    digitalWrite(buzzer, LOW);
    
    while (digitalRead(footPedal) == LOW);
    MCSpoolOut();
    digitalWrite(MCVarSpeed, HIGH);
    digitalWrite(MCVSA, HIGH);
    while (digitalRead(footPedal) == HIGH);
    MCShutOff();
    
    bottomVal = WECounts;
       
    sameCount = 0;
    lastCount = WECounts;
    delay(100);
    while(sameCount < 10)
    {
      if (WECounts == lastCount) sameCount++;
      lastCount = WECounts;
      delay(100);
    }
    bottomVal = lastCount;
    digitalWrite(buzzer, HIGH);
    delay(500);
    digitalWrite(buzzer, LOW);
    
    // Error Checking
    int errorCheck = topVal - bottomVal;
    if (errorCheck <= 0)
    {
      lcd.setCursor(0,0);
      lcd.print("Error:              ");
      lcd.setCursor(0,1);
      lcd.print("Calibration Failed. ");
      lcd.setCursor(0,2);
      lcd.print("Please try again.   ");
      lcd.setCursor(0,3);
      lcd.print("                    ");
      calibrateFlag = false;
      return;  // exit calibrate function
    }
    
    // print values to screen
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Top of Reach: ");
    lcd.print(topVal);
    lcd.setCursor(0,1);
    lcd.print("Chest Val: ");
    lcd.print(bottomVal);
    lcd.setCursor(0,2);
    lcd.print("Correct?");
    
    while(true)
    {
      if (digitalRead(navBtn) == LOW)
      {
        calibrateFlag = true;
        break;
      }
      if (digitalRead(selBtn) == LOW)
      {
        calibrateFlag = false;
        break;
      }
    }
  }
  else
  {
    // Print error message
    lcd.setCursor(0,0);
    lcd.print("Error:              ");
    lcd.setCursor(0,1);
    lcd.print("Calibration Failed. ");
    lcd.setCursor(0,2);
    lcd.print("Unload bar & ensure "); 
    lcd.setCursor(0,3);
    lcd.print("pedal is pressed.   ");
    digitalWrite(buzzer, HIGH);
    delay(1000);
    digitalWrite(buzzer, LOW);
  }
}
    
void assist(int level)
{
  if (WECounts < 0)
  {
    if (level == 1)
    {
      MCReelIn();
      digitalWrite(MCVarSpeed, HIGH);
      digitalWrite(MCVSA, HIGH);
      digitalWrite(MCVSB, LOW);
      helpLevel = 1;
    }
    else if (level == 2)
    {
      MCReelIn();
      digitalWrite(MCVarSpeed, HIGH);
      digitalWrite(MCVSA, LOW);
      digitalWrite(MCVSB, HIGH);
      helpLevel = 2;
    }
    else
    {
      digitalWrite(MCVarSpeed, LOW);
      digitalWrite(MCVSA, LOW);
      digitalWrite(MCVSB, LOW);
      emergencyLift();
    }
  }
}

void lift()
{
  static byte liftState = barInRack;
  static boolean startFlag = false;
  
  if (calibrateFlag)
  {
    while (liftingFlag)  
    {
      if (digitalRead(footPedal) == HIGH)
      {
        emergencyLift();
        break;
      }
      if (startFlag && (analogRead(pressureSensor) > 1))
      {
        liftingFlag = false;
        break;
      }
      switch (liftState)
      {
        case barInRack:
          if (analogRead(pressureSensor) < 1)
          {
            startFlag = true;
            liftState = upwardState;
          }
          break;
          
        case downwardState:
          if (upwards)
          {
            liftState = upwardState;
            break;
          }
          if (countDiff > speedThreshold)
          {
            emergencyLift();
            break;
          }
          if (WECounts == bottomVal)
          {
            liftState = upwardState;
            break;
          }
          
          break;
          
        case upwardState:
          if (downwards)
          {
            emergencyLift();
            break;
          }
          else if (stall)
          {
            if(WECounts != topVal)
            {
              assist(stall);
              break;
            }
          }
          break;
          
        default:
          // shouldn't get here
          break;
      }
      
      delay(100);
    }
  }
  else
  {
    lcd.setCursor(0,0);
    lcd.print("       Error:       ");
    lcd.setCursor(0,1);
    lcd.print("  Please Calibrate  ");
    lcd.setCursor(0,2);
    lcd.print("        First       ");
    lcd.setCursor(0,3);
    lcd.print("                    ");
    returnToMainMenu = true;
  }
}

void statistics()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Statistics:         ");
  lcd.setCursor(0,1);
  lcd.print("Weight:");
  lcd.print(weight);
  lcd.setCursor(0,2);
  lcd.print("Help: ");
  lcd.print(helpLevel);
  
  while((digitalRead(navBtn) == HIGH) && (digitalRead(selBtn) == HIGH));
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
      if (menuRow == 1)
      {
        comingSoon();
        delay(3000);
        //calibrate();
      }
      else if (menuRow == 2)
      {
        comingSoon();
        delay(3000);
        //liftingFlag = true;
        //lift();
      }
      else if (menuRow == 3)
      {
        comingSoon();
        delay(3000);
        //statistics();
      }
      
      
      setupMenu();
      myState = waitForInput;
      

      break;
      
    default:
      // blink LED to indicate error
      //digitalWrite(led, !digitalRead(led));
      break;
  }
  
  noInterrupts();
  int tmpCnt = WECounts;
  int tmpErr = WEError;
  interrupts();
  
  Serial.print("WE Counts: ");
  Serial.print(tmpCnt);
  Serial.println();
//  Serial.print("WE Error: ");
//  Serial.print(tmpErr);
//  Serial.println();
//  Serial.println();  
  
  delay(100);
}

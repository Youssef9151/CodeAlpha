#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

Servo actualS1; 
Servo actualS2;
Servo grabberServo; 

// Pins
const int pot1Pin = A0;      
const int pot2Pin = A1;      
const int recordButton = 6;
const int grabberButton = 7; 
const int servo1Pin = 9;
const int servo2Pin = 10;
const int grabberPin = 11; 

// Ultrasonic Pins
const int trigPin = 8; 
const int echoPin = 12; 

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Memory & Logic
const int maxSteps = 100;
int steps1[maxSteps]; 
int steps2[maxSteps];
int stepsGrabber[maxSteps]; 
unsigned long stepDelays[maxSteps]; 
int stepCount = 0;
bool isPlaying = false;

// Tracking
int angle1 = 90; 
int angle2 = 90; 
bool isGrabberClosed = false; 
const int grabberOpenAngle = 120; 
const int grabberClosedAngle = 0; 
bool lastGrabberBtnState = HIGH; 
unsigned long lastRecordTime = 0; 

// توقيت تحديث الشاشة
unsigned long lastLcdUpdate = 0;

// Safety & Smoothness Settings
const int safetyDistance = 15; // زيادة المسافة قليلاً للأمان
const int manualMoveSpeed = 2; // سرعة الحركة التدريجية في وضع التسجيل (درجة في كل لفة)

// --- HELPER: SMOOTH POTENTIOMETER READ ---
int getSmoothRead(int pin) {
  long total = 0;
  for(int i = 0; i < 10; i++) {
    total += analogRead(pin);
    delay(2); 
  }
  return total / 10;
}

// --- HELPER: UPDATE LCD WITHOUT FLICKER ---
void updateLCD(String statusMsg) {
  lcd.setCursor(0, 0);
  lcd.print("S1:"); 
  if(angle1 < 100) lcd.print(" ");
  if(angle1 < 10) lcd.print(" ");
  lcd.print(angle1);
  
  lcd.print(" S2:"); 
  if(angle2 < 100) lcd.print(" ");
  if(angle2 < 10) lcd.print(" ");
  lcd.print(angle2);

  lcd.setCursor(0, 1);
  String paddedMsg = statusMsg;
  while(paddedMsg.length() < 16) {
    paddedMsg += " ";
  }
  lcd.print(paddedMsg);
}

// --- HELPER: GET DISTANCE ---
long getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 20000); 
  long distance = duration * 0.034 / 2;
  
  if (distance <= 0) return 999; 
  return distance;
}

void setup() {
  pinMode(recordButton, INPUT_PULLUP);
  pinMode(grabberButton, INPUT_PULLUP); 
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // القراءة الأولية
  int initialPot1 = getSmoothRead(pot1Pin);
  int initialPot2 = getSmoothRead(pot2Pin);
  
  angle1 = constrain(map(initialPot1, 0, 1023, 0, 180), 0, 180);
  angle2 = constrain(map(initialPot2, 0, 1023, 0, 180), 0, 180);

  actualS1.attach(servo1Pin);
  actualS2.attach(servo2Pin);
  grabberServo.attach(grabberPin); 

  actualS1.write(angle1);
  actualS2.write(angle2);
  grabberServo.write(grabberOpenAngle);
  
  updateLCD("System Ready");
}

void loop() {
  long currentDist = getDistance();

  // --- 1. EMERGENCY CHECK ---
  if (currentDist < safetyDistance) {
    updateLCD("STOP! DIST:" + String(currentDist) + "cm");
    delay(100); 
    return; // يخرج من الـ loop ولا ينفذ أي حركة     
  }

  if (!isPlaying) {
    // --- PHASE 1: RECORDING (WITH SMOOTH MANUAL MOVEMENT) ---
    int potVal1 = getSmoothRead(pot1Pin);
    int potVal2 = getSmoothRead(pot2Pin);
    
    int targetAngle1 = constrain(map(potVal1, 0, 1023, 0, 180), 0, 180);
    int targetAngle2 = constrain(map(potVal2, 0, 1023, 0, 180), 0, 180);

    bool isMoving = false;

    // تحريك Servo 1 تدريجياً نحو قيمة الـ Potentiometer
    if (abs(angle1 - targetAngle1) > 2) {
      if (angle1 < targetAngle1) angle1 += manualMoveSpeed;
      else if (angle1 > targetAngle1) angle1 -= manualMoveSpeed;
      actualS1.write(angle1);
      isMoving = true;
    }

    // تحريك Servo 2 تدريجياً نحو قيمة الـ Potentiometer
    if (abs(angle2 - targetAngle2) > 2) {
      if (angle2 < targetAngle2) angle2 += manualMoveSpeed;
      else if (angle2 > targetAngle2) angle2 -= manualMoveSpeed;
      actualS2.write(angle2);
      isMoving = true;
    }

    // تحديث الشاشة أثناء الحركة اليدوية
    if (isMoving && (millis() - lastLcdUpdate > 100)) {
      updateLCD("Recording...");
      lastLcdUpdate = millis();
    }

    // زر الكلابشة (الماسك)
    bool currentGrabberBtnState = digitalRead(grabberButton);
    if (currentGrabberBtnState == LOW && lastGrabberBtnState == HIGH) {
      isGrabberClosed = !isGrabberClosed; 
      grabberServo.write(isGrabberClosed ? grabberClosedAngle : grabberOpenAngle);
      updateLCD(isGrabberClosed ? "Claw: CLOSED" : "Claw: OPEN");
      delay(200); 
    }
    lastGrabberBtnState = currentGrabberBtnState;

    // زر التسجيل والتشغيل
    if (digitalRead(recordButton) == LOW) {
      unsigned long startTime = millis();
      while(digitalRead(recordButton) == LOW) {
        if (millis() - startTime > 2000) { 
          isPlaying = true;
          updateLCD("Starting Loop...");
          delay(1000);
          break; 
        }
      }

      if (!isPlaying && stepCount < maxSteps) {
        unsigned long timeNow = millis();
        unsigned long timeWaited = (stepCount > 0) ? (timeNow - lastRecordTime) : 0;
        if (timeWaited > 5000) timeWaited = 5000; 

        steps1[stepCount] = angle1;
        steps2[stepCount] = angle2;
        stepsGrabber[stepCount] = isGrabberClosed ? grabberClosedAngle : grabberOpenAngle;
        stepDelays[stepCount] = timeWaited;
        
        stepCount++;
        lastRecordTime = millis(); 
        updateLCD("Step " + String(stepCount) + " Saved!");
        delay(500); 
      }
    }
    
    delay(15); // تأخير بسيط لجعل الحركة اليدوية سلسة وغير مهتزة

  } else {
    // --- PHASE 2: PLAYBACK ---
    for (int i = 0; i < stepCount; i++) {
      if (getDistance() < safetyDistance) {
          isPlaying = false; 
          updateLCD("STOP! OBSTACLE");
          return; 
      }

      if (i > 0) delay(stepDelays[i]); 
      
      int target1 = steps1[i];
      int target2 = steps2[i];
      updateLCD("Playing Step " + String(i + 1));

      // حركة ناعمة في وضع التشغيل
      while (angle1 != target1 || angle2 != target2) {
        if (getDistance() < safetyDistance) {
            isPlaying = false; 
            updateLCD("EMERGENCY STOP!");
            return; 
        }

        if (abs(angle1 - target1) <= manualMoveSpeed) angle1 = target1;
        else if (angle1 < target1) angle1 += manualMoveSpeed;
        else if (angle1 > target1) angle1 -= manualMoveSpeed;

        if (abs(angle2 - target2) <= manualMoveSpeed) angle2 = target2;
        else if (angle2 < target2) angle2 += manualMoveSpeed;
        else if (angle2 > target2) angle2 -= manualMoveSpeed;

        actualS1.write(angle1);
        actualS2.write(angle2);
        delay(20); 
      }
      grabberServo.write(stepsGrabber[i]);
    } 
    delay(1000); 
  }
}
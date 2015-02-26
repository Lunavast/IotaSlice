//
// IOTA 3d printer firmware
//

#include "Arduino.h"

#if 0
trick the Arduino software into putting prototypes here, or they will cause an error!
#endif
#line 10

// TOC:
// - configuration
// -- globals
// -- forwards
// - hardware abstraction layer
// -- display
// -- keys
// -- beeper
// -- steppers and endstops
// -- motors
// -- ink
// - .3dp interpreter
// -- SD Card file system
// -- functions and lookup
// - firmware
// -- menus
// -- screens
// - setup and main
// -- setup
// -- main
 
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <string.h>

// ================ configuration
// ================ globals


int gKeysDown = 0;
int gKeysLast = 0;


// ================ forwards

typedef void (Callback)(void*);
typedef Callback *CallbackPtr;

extern unsigned char simple_font[128][13];

void beeperBleep();
void firmwareError(const char *line1, const char *line2="", const char *line3="");

// ================ hardware abstraction layer

// ---------------- display

const int displayAddress = 0x63;

void displayBacklight(int onOff)
{
  Wire.beginTransmission(displayAddress);
  Wire.write(0);
  if (onOff==0)
    Wire.write(20); // backlight off
  else
    Wire.write(19); // backlight on
  Wire.endTransmission();
}

void displayClear()
{
  Wire.beginTransmission(displayAddress);
  Wire.write(0);
  Wire.write(12);
  Wire.endTransmission();
}

void displayHome()
{
  Wire.beginTransmission(displayAddress);
  Wire.write(0);
  Wire.write(1);
  Wire.endTransmission();
}

void displayNAt(int n, int row, int col, const char *text)
{
  int i;
  Wire.beginTransmission(displayAddress);
  Wire.write(0);
  Wire.write(3);
  Wire.write(row);
  Wire.write(col);
  for (i=0; i<n; i++) {
    char c = *text++;
    if (c==0) break;
    Wire.write(c);
  }
  for (; i<n; i++) {
    Wire.write(' ');
  }
  Wire.endTransmission();
}

void displayAt(int row, int col, const char *text)
{
  Wire.beginTransmission(displayAddress);
  Wire.write(0);
  Wire.write(3);
  Wire.write(row);
  Wire.write(col);
  Wire.write(text);
  Wire.endTransmission();
}

void displayDebug(const char *str)
{
  static int ball = 0;
  static const char ball_lut[] = { '-', '`', '|', '/' };
  char buf[2] = { ball_lut[ball], 0 }; ball = (ball+1) & 0x3;
  displayAt(1, 11, "*");
  displayNAt(8, 1, 12, str);
  displayAt(1, 20, buf);
}

void displayDebug(long v)
{
  char buf[12];
  ltoa(v, buf, 16);
  displayDebug(buf); delay(50);
}

void displayScreen(const char *line1, const char *line2, const char *line3, const char *line4)
{
  displayNAt(20, 1, 1, line1); delay(10);
  displayNAt(20, 2, 1, line2); delay(10);
  displayNAt(20, 3, 1, line3); delay(10);
  displayNAt(20, 4, 1, line4); delay(10);
}

void displayAboutScreen()
{
  beeperBleep();
  displayScreen(
                "+----  About:       ",
                "| IOTA 3DP   v0.3a  ",
                "   matthiasm.com   |",
                "     (c) 2013  ----+");
}

// set up the i2c LCD03 display
void setupDisplay()
{
  Wire.begin();         // become an I2C master
  Wire.beginTransmission(displayAddress);
  Wire.write(0);
  Wire.write(4); // hide cursor
  Wire.endTransmission();
  delay(10);
  displayBacklight(1);
  delay(10);
  displayAboutScreen();
}


// ---------------- keys

const int kKeyHome = 0x0100;
const int kKeyPlay = 0x0002;
const int kKeyBack = 0x2000;
const int kKeyUp   = 0x0001;
const int kKeyDown = 0x0008;
const int kKeyOK   = 0x0400;

int keysScan()
{
  int keysDownNow = 0;
  Wire.beginTransmission(displayAddress);
  Wire.write(1);
  Wire.endTransmission();
  Wire.requestFrom(displayAddress, 1);
  if (Wire.available()) keysDownNow |= (Wire.read()<<8);
  Wire.beginTransmission(displayAddress);
  Wire.write(2);
  Wire.endTransmission();
  Wire.requestFrom(displayAddress, 1);
  if (Wire.available()) keysDownNow |= (Wire.read());
  gKeysLast = (gKeysDown ^ keysDownNow) & keysDownNow;
  gKeysDown = keysDownNow;
  return gKeysLast;
}

// set up the i2c LCD03 key matrix
void setupKeys()
{
}


// ---------------- beeper

const int beeperPin = 12;

void beeperBlip()
{
  int i;
  for (i=0; i<50; i++) {
    digitalWrite(beeperPin, 1);
    delayMicroseconds(100);
    digitalWrite(beeperPin, 0);
    delayMicroseconds(100);
  }
}

void beeperBeep(int len, int freq)
{
  int i;
  for (i=0; i<len; i++) {
    digitalWrite(beeperPin, 1);
    delayMicroseconds(freq);
    digitalWrite(beeperPin, 0);
    delayMicroseconds(freq);
  }
}

void beeperBleep()
{
  beeperBeep(100, 100);
  beeperBeep(100, 120);
  beeperBeep(100, 140);
  beeperBeep(100, 120);
  beeperBeep(100, 100);
}

// set up the ports to run the beeper to make crude sounds
void setupBeeper()
{
  digitalWrite(beeperPin, 0);
  pinMode(beeperPin, OUTPUT);
  digitalWrite(13, 0);
  pinMode(13, OUTPUT); // LED
}


// ---------------- motors

// ---- roller stepper motor
const int stepperRDirPin = 29;
const int stepperRStepPin = 28;
const int stepperREnablePin = 2;
const int stepperRDelay = 30;

void motorRoller(long t, int aDelay=stepperRDelay, int dir=1)
{
  long i;
  digitalWrite(stepperRDirPin, dir);
  for (i=0; i<t*400; i++) {
    digitalWrite(stepperRStepPin, 0);
    delayMicroseconds(aDelay);
    digitalWrite(stepperRStepPin, 1);
    delayMicroseconds(aDelay);
  }
}

void stepperGotoY(long);
void stepperPowerOn(int);

void motorRollerClean()
{
  stepperGotoY(4200);
  stepperPowerOn(0x10);
  //motorRoller(100, stepperRDelay, 1);
  motorRoller(100, stepperRDelay, 0);
  //motorRoller(100, stepperRDelay, 1);
}

// set up the ports to run the DC motors
void setupMotors()
{
  pinMode(stepperRDirPin, OUTPUT);  //
  pinMode(stepperRStepPin, OUTPUT);  //
  digitalWrite(stepperREnablePin, 1);
  pinMode(stepperREnablePin, OUTPUT);  //
}


// ---------------- steppers and endstops

// ---- x axis stepper motor (0...18500)
const int stepperXDirPin = 33;
const int stepperXStepPin = 32;
const int stepperXEnablePin = 30;
const int stepperXEndstopPin = 31;
const int stepperXDelay = 40;

// ---- y axis stepper motor (6500...28500...49500 Hopper, x...25536 Ink Into Hopper 2)
const int stepperYDirPin = 37;
const int stepperYStepPin = 36;
const int stepperYEnablePin = 34;
const int stepperYEndstopPin = 35;
const int stepperYDelay = 140;  // 120
const int stepperYDelayFast = 80;

// ---- z1 axis stepper motor
const int stepperZ1DirPin = 41;
const int stepperZ1StepPin = 40;
const int stepperZ1EnablePin = 38;
const int stepperZ1EndstopPin = 43;
const int stepperZ1Delay = 40;

// ---- z2 axis stepper motor
const int stepperZ2DirPin = 45;
const int stepperZ2StepPin = 44;
const int stepperZ2EnablePin = 42;
const int stepperZ2EndstopPin = 43;
const int stepperZ2Delay = 40;

// ---- stepper management
long gStepperCurrentX = 0;
boolean gStepperCurrentXKnown = false;
long gStepperCurrentY = 0;
boolean gStepperCurrentYKnown = false;
long gStepperCurrentZ1 = 0;
long gStepperCurrentZ2 = 0;
int gStepperPowerMap = 0; // all off

void stepperPowerOff(int mask)
{
  int todo = (((~gStepperPowerMap)^mask)&mask);
  if (todo) {
    if (todo&0x0001) digitalWrite(stepperXEnablePin, 1);
    if (todo&0x0002) digitalWrite(stepperYEnablePin, 1);
    if (todo&0x0004) digitalWrite(stepperZ1EnablePin, 1);
    if (todo&0x0008) digitalWrite(stepperZ2EnablePin, 1);
    if (todo&0x0010) digitalWrite(stepperREnablePin, 1);
    gStepperPowerMap &= ~mask;
  }
}

void stepperPowerOn(int mask)
{
  int todo = ((gStepperPowerMap^mask)&mask);
  if (todo) {
    if (todo&0x0001) digitalWrite(stepperXEnablePin, 0);
    if (todo&0x0002) digitalWrite(stepperYEnablePin, 0);
    if (todo&0x0004) digitalWrite(stepperZ1EnablePin, 0);
    if (todo&0x0008) digitalWrite(stepperZ2EnablePin, 0);
    if (todo&0x0010) digitalWrite(stepperREnablePin, 0);
    gStepperPowerMap |= mask;
    delay(1500); // give the stepper driver time to figure out the power setup
  }
}

void stepperPowerX(int onOff)
{
  if (onOff)
    stepperPowerOn(0x0001);
  else
    stepperPowerOff(0x0001);
}

void stepperHomeX()
{
  int i;
  stepperPowerOn(0x0001);
  // first make sure that we are not already in the home zone
  digitalWrite(stepperXDirPin, 0); // increment
  for (i=0; i<1000; i++) {
    if (!digitalRead(stepperXEndstopPin)) {
      break;
    }
    digitalWrite(stepperXStepPin, 0);
    delayMicroseconds(stepperXDelay);
    digitalWrite(stepperXStepPin, 1);
    delayMicroseconds(stepperXDelay);
  }
  // now find the beginning of the home zone
  digitalWrite(stepperXDirPin, 1); // decrement
  for (i=0; i<100000; i++) {
    if (digitalRead(stepperXEndstopPin)) {
      break;
    }
    digitalWrite(stepperXStepPin, 0);
    delayMicroseconds(stepperXDelay);
    digitalWrite(stepperXStepPin, 1);
    delayMicroseconds(stepperXDelay);
  }
  gStepperCurrentX = 0;
  gStepperCurrentXKnown = true;
}

void stepperMoveX(long dx)
{
  long i;
  stepperPowerOn(0x0001);
  gStepperCurrentX += dx;
  if (dx<0) {
    digitalWrite(stepperXDirPin, 1); // decrement
    dx = -dx;
  } else {
    digitalWrite(stepperXDirPin, 0); // increment
  }
  for (i=0; i<dx; i++) {
    digitalWrite(stepperXStepPin, 0);
    delayMicroseconds(stepperXDelay);
    digitalWrite(stepperXStepPin, 1);
    delayMicroseconds(stepperXDelay);
  }
}

void stepperGotoX(long x)
{
  if (!gStepperCurrentXKnown) stepperHomeX();
  stepperMoveX(x-gStepperCurrentX);
}

void stepperPowerY(int onOff)
{
  if (onOff)
    stepperPowerOn(0x0002);
  else
    stepperPowerOff(0x0002);
}

void stepperMoveY(long dy);
void stepperGotoY(long y);

void stepperHomeY()
{
  int i;
  stepperPowerOn(0x0002);
  // if we know the Y home position, accelerate the job and drive close to the Y stop first
  if (gStepperCurrentYKnown) {
    if (gStepperCurrentY>1350*3) // sefatyzone is 3cm
      stepperGotoY(1350*3);
  }
  digitalWrite(stepperYDirPin, 0); // decrement
  for (i=0; i<100000; i++) {
    if (digitalRead(stepperYEndstopPin)) {
      break;
    }
    digitalWrite(stepperYStepPin, 0);
    delayMicroseconds(stepperYDelay);
    digitalWrite(stepperYStepPin, 1);
    delayMicroseconds(stepperYDelay);
  }
  gStepperCurrentY = 0;
  gStepperCurrentYKnown = true;
}

void stepperMoveY(long dy)
{
  long i;
  stepperPowerOn(0x0002);
  gStepperCurrentY += dy;
  if (dy<0) {
    digitalWrite(stepperYDirPin, 0); // decrement
    dy = -dy;
  } else {
    digitalWrite(stepperYDirPin, 1); // increment
  }
  int accel = stepperYDelay - stepperYDelayFast;
  int rampUp, constSpeed, rampDown, currentDelay;
  if (2*accel > dy) {
    rampUp = dy/2;
    rampDown = dy-rampUp;
    constSpeed = 0;
  } else {
    rampUp = accel;
    rampDown = accel;
    constSpeed = dy - rampUp - rampDown;
  }
  // ramp up
  currentDelay = stepperYDelay;
  for (i=0; i<rampUp; i++) {
    digitalWrite(stepperYStepPin, 0);
    delayMicroseconds(currentDelay);
    digitalWrite(stepperYStepPin, 1);
    delayMicroseconds(currentDelay);
    currentDelay--;
  }
  // const speed
  for (i=0; i<constSpeed; i++) {
    digitalWrite(stepperYStepPin, 0);
    delayMicroseconds(currentDelay);
    digitalWrite(stepperYStepPin, 1);
    delayMicroseconds(currentDelay);
  }
  // ramp udown
  for (i=0; i<rampDown; i++) {
    digitalWrite(stepperYStepPin, 0);
    delayMicroseconds(currentDelay);
    digitalWrite(stepperYStepPin, 1);
    delayMicroseconds(currentDelay);
    currentDelay++;
  }
}

void stepperGotoY(long y)
{
  if (!gStepperCurrentYKnown) stepperHomeY();
  stepperMoveY(y-gStepperCurrentY);
}

void stepperSpreadTo(long y)
{
  long i;
  int j, rd = stepperYDelay / 4;
  long dy = y-gStepperCurrentY;
  stepperPowerOn(0x0012);
  gStepperCurrentY += dy;
  if (dy<0) {
    digitalWrite(stepperYDirPin, 0);
    digitalWrite(stepperRDirPin, 1);
    dy = -dy;
  } else {
    digitalWrite(stepperYDirPin, 1);
    digitalWrite(stepperRDirPin, 0);
  }
  for (i=0; i<dy; i++) {
    for (j=0; j<4; j++) {
      if (j==0)
        digitalWrite(stepperYStepPin, 0);
      digitalWrite(stepperRStepPin, 0);
      delayMicroseconds(rd);
      if (j==0)
        digitalWrite(stepperYStepPin, 1);
      digitalWrite(stepperRStepPin, 1);
      delayMicroseconds(rd);
    }
  }
}

void stepperPowerZ1(int onOff)
{
  if (onOff)
    stepperPowerOn(0x0004);
  else
    stepperPowerOff(0x0004);
}

void stepperHomeZ1()
{
  int i;
  stepperPowerOn(0x0004);
  digitalWrite(stepperZ1DirPin, 1); // decrement
  for (i=0; i<100000; i++) {
    if (digitalRead(stepperZ1EndstopPin)) {
      break;
    }
    digitalWrite(stepperZ1StepPin, 0);
    delayMicroseconds(stepperZ1Delay);
    digitalWrite(stepperZ1StepPin, 1);
    delayMicroseconds(stepperZ1Delay);
  }
  gStepperCurrentZ1 = 0;
}

void stepperMoveZ1(long dx)
{
  long i;
  stepperPowerOn(0x0004);
  gStepperCurrentZ1 += dx;
  if (dx<0) {
    digitalWrite(stepperZ1DirPin, 1); // decrement
    dx = -dx;
  } else {
    digitalWrite(stepperZ1DirPin, 0); // increment
  }
  for (i=0; i<dx; i++) {
    digitalWrite(stepperZ1StepPin, 0);
    delayMicroseconds(stepperZ1Delay);
    digitalWrite(stepperZ1StepPin, 1);
    delayMicroseconds(stepperZ1Delay);
  }
}

void stepperGotoZ1(long x)
{
  stepperMoveZ1(x-gStepperCurrentZ1);
}

void stepperPowerZ2(int onOff)
{
  if (onOff)
    stepperPowerOn(0x0008);
  else
    stepperPowerOff(0x0008);
}

void stepperHomeZ2()
{
  int i;
  stepperPowerOn(0x0008);
  digitalWrite(stepperZ2DirPin, 1); // decrement
  for (i=0; i<100000; i++) {
    if (digitalRead(stepperZ2EndstopPin)) {
      break;
    }
    digitalWrite(stepperZ2StepPin, 0);
    delayMicroseconds(stepperZ2Delay);
    digitalWrite(stepperZ2StepPin, 1);
    delayMicroseconds(stepperZ2Delay);
  }
  gStepperCurrentZ2 = 0;
}

void stepperMoveZ2(long dx)
{
  long i;
  stepperPowerOn(0x0008);
  gStepperCurrentZ2 += dx;
  if (dx<0) {
    digitalWrite(stepperZ2DirPin, 0); // decrement
    dx = -dx;
  } else {
    digitalWrite(stepperZ2DirPin, 1); // increment
  }
  for (i=0; i<dx; i++) {
    digitalWrite(stepperZ2StepPin, 0);
    delayMicroseconds(stepperZ2Delay);
    digitalWrite(stepperZ2StepPin, 1);
    delayMicroseconds(stepperZ2Delay);
  }
}

void stepperGotoZ2(long x)
{
  stepperMoveZ2(x-gStepperCurrentZ2);
}

// set up the ports to run the stepper motors
void setupSteppers()
{
  pinMode(stepperXDirPin, OUTPUT);  //
  pinMode(stepperXStepPin, OUTPUT);  //
  digitalWrite(stepperXEnablePin, 1);
  pinMode(stepperXEnablePin, OUTPUT);  //
  pinMode(stepperXEndstopPin, INPUT_PULLUP);  //
  
  pinMode(stepperYDirPin, OUTPUT);  //
  pinMode(stepperYStepPin, OUTPUT);  //
  digitalWrite(stepperYEnablePin, 1);
  pinMode(stepperYEnablePin, OUTPUT);  //
  pinMode(stepperYEndstopPin, INPUT_PULLUP);  //
  
  pinMode(stepperZ1DirPin, OUTPUT);  //
  pinMode(stepperZ1StepPin, OUTPUT);  //
  digitalWrite(stepperZ1EnablePin, 1);
  pinMode(stepperZ1EnablePin, OUTPUT);  //
  pinMode(stepperZ1EndstopPin, INPUT_PULLUP);  //
  
  pinMode(stepperZ2DirPin, OUTPUT);  //
  pinMode(stepperZ2StepPin, OUTPUT);  //
  digitalWrite(stepperZ2EnablePin, 1);
  pinMode(stepperZ2EnablePin, OUTPUT);  //
  pinMode(stepperZ2EndstopPin, INPUT_PULLUP);  //
}

// dz is int 100th mm
void stepperSpreadLayer(long dz) {
  stepperPowerOn(0x001f);          // power on all needed motors (all of them, actually)
  stepperHomeX();                  // move the head to the left where it might get less dirty
  stepperPowerOff(0x0001);         // we are done with the x axis
  stepperHomeY();                  // now home the y axis, so we have a zero reference
  stepperMoveZ1((-100)*16);        // lower the supply piston to loosen up the powder
  stepperMoveZ1((100+50+dz)*16);   // raise the supply piston, so a 'dz' mm layer gets scraped off
  stepperMoveZ2((50)*16);          // raise the build piston a bit, so the powder is evenly spread
  motorRollerClean();
  stepperGotoY(6500);              // position the y axis in front of the supply chamber
  stepperSpreadTo(49500);            // now move y and roll to the maximum y position
  stepperMoveZ2((-50)*16);         // lower the build piston a bit, so the powder is not disturbed when y moves back
  stepperMoveZ1((-50)*16);         // same for the supply piston
}



// ---------------- ink

const int inkClockPin = 47;
const int inkDataPin = 46;
const int inkEnablePin = 48;
const int inkStrobePin = 49;

void inkFireNozzle(int n)
{
  //static unsigned char lut[] = { 3, 10, 2, 9, 0, 8, 1, 5, 6, 7, 4, 11, 15, 15, 15, 15 };
  static unsigned char lut[] = { 3, 1, 10, 5, 2, 6, 9, 7, 0, 4, 8, 11, 15, 15, 15, 15 };
  int i;
  n = lut[n&15];
  // shift the value in
  for (i=15; i>=0; i--) {
    if (i==n) {
      digitalWrite(inkDataPin, 1);
    } else {
      digitalWrite(inkDataPin, 0);
    }
    digitalWrite(inkClockPin, 1);
    digitalWrite(inkClockPin, 0);
  }
  // fire what we shifted in
  digitalWrite(inkStrobePin, 0);
  digitalWrite(inkStrobePin, 1);
  noInterrupts();
  //digitalWrite(inkEnablePin, 0);
  PORTL &= B11111101;
  delayMicroseconds(4);
  PORTL |= B00000010;
  //digitalWrite(inkEnablePin, 1);
  interrupts();
}

void inkFirePattern1(int pattern, int repeat)
{
  int h, j;
  for (h=0; h<repeat; h++) {
    int p = pattern;
    for (j=0; j<12; j++) {
      if (p&1) {
        inkFireNozzle(j);
      } else {
        inkFireNozzle(15);
      }
      p = p>>1;
      stepperMoveX(3);
    }
  }
}

void inkFirePattern2(int pattern, int repeat)
{
  int h, i, j;
  for (h=0; h<repeat; h++) {
    for (i=0; i<2; i++) {
      int p = pattern;
      for (j=0; j<12; j++) {
        if (p&1) {
          inkFireNozzle(j);
        } else {
          inkFireNozzle(15);
        }
        p = p>>1;
        if (j&1) {
          stepperMoveX(1);
          delayMicroseconds(400);
        } else {
          stepperMoveX(2);
          delayMicroseconds(400);
        }
      }
    }
  }
}

// 12 steps per pattern, repeat that, advance is currently unused
// 12 steps = 0,089064mm, reoeat3 = 0,267192mm
void inkFirePattern3(int pattern, int repeat)
{
  int h, i, j;
  for (h=0; h<repeat; h++) {
    for (i=0; i<3; i++) {
      int p = pattern;
      for (j=0; j<12; j++) {
        if (p&1) {
          inkFireNozzle(j);
        } else {
          inkFireNozzle(15);
        }
        p = p>>1;
        stepperMoveX(1);
        delayMicroseconds(4*stepperXDelay);
      }
    }
  }
}

void inkFirePattern(int pattern, int drops, int repeat=1)
{
  int h, i, j;
  
  // reorganzie pattern so that neigboring nozzles do not fire in rapid order
  int p1 = pattern;
  pattern = 0;
  if (p1 & 0x001) pattern |= 0x001;
  if (p1 & 0x002) pattern |= 0x004;
  if (p1 & 0x004) pattern |= 0x010;
  if (p1 & 0x008) pattern |= 0x040;
  if (p1 & 0x010) pattern |= 0x100;
  if (p1 & 0x020) pattern |= 0x400;
  if (p1 & 0x040) pattern |= 0x002;
  if (p1 & 0x080) pattern |= 0x008;
  if (p1 & 0x100) pattern |= 0x020;
  if (p1 & 0x200) pattern |= 0x080;
  if (p1 & 0x400) pattern |= 0x200;
  if (p1 & 0x800) pattern |= 0x800;
  
  if (drops==1) return inkFirePattern1(pattern, repeat);
  if (drops==2) return inkFirePattern2(pattern, repeat);
  if (drops==3) return inkFirePattern3(pattern, repeat);
  for (h=0; h<repeat; h++) {
    int x0 = 0, x1 = 36, y0 = 0, y1 = 12*drops;
    int dx = x1-x0, dy = y1-y0;
    int err = dy/2;
    int ystep;
    int y = y0;
    for (i=0; i<drops; i++) {
      int p = pattern;
      for (j=0; j<12; j++) {
        if (p&1) {
          inkFireNozzle(j);
        } else {
          inkFireNozzle(15);
        }
        p = p>>1;
        // bresenham
        err = err - dx;
        if (err<0) {
          err += dy;
          stepperMoveX(1);
          delayMicroseconds(4*stepperXDelay);
        } else {
          delayMicroseconds(6*stepperXDelay);
        }
      }
    }
  }
}

void inkChar(char c)
{
  int i, j;
  c = c & 127;
  unsigned char *f = simple_font[c];
  for (i=0; i<8; i++) {
    for (j=0; j<13; j++) {
      unsigned char b = f[12-j]>>(7-i);
      if (b&1) {
        inkFireNozzle(j);
      } else {
        inkFireNozzle(15);
      }
      stepperMoveX(3);
    }
  }
}

void inkWrite(char *text)
{
  char *s = text;
  for (;;) {
    char c = *s++;
    if (c==0) break;
    inkChar(c);
  }
}

// set up the ports to run the ink cartridge
void setupInk()
{
  digitalWrite(inkEnablePin, 1);
  pinMode(inkClockPin, OUTPUT);
  pinMode(inkDataPin, OUTPUT);
  pinMode(inkEnablePin, OUTPUT);
  pinMode(inkStrobePin, OUTPUT);
}


// ---------------- drive

const int driveSelectPin = 53;
const int driveDetectPin = 3;

// set up the ports to run the SD Card as a drive
void setupDrive()
{
  pinMode(driveSelectPin, OUTPUT);
}


// ================ .3dp interpreter

File gInterpreterFile;

// ---------------- SD Card file system

int interpreterReadShort()
{
  int ub, ret = 0;
  // read bit 0..6
  ub = gInterpreterFile.read();
  if (ub==-1) return -1; // should not happen
  ret = ub & 0x7f;
  if (ub&0x80) {
    // shift to 13..7 and read bit 0..6
    ub = gInterpreterFile.read();
    ret = (ret<<7) | (ub & 0x7f);
  }
  if (ub&0x80) {
    // shift to 20..14 (some bits lost) and read bit 0..6
    ub = gInterpreterFile.read();
    ret = (ret<<7) | (ub & 0x7f);
  }
  return ret;
}

long interpreterReadLong()
{
  int ub;
  long ret = 0;
  // read bit 0..6
  ub = gInterpreterFile.read();
  if (ub==-1) return -1; // should not happen
  ret = ub & 0x7f;
  if (ub&0x80) {
    // shift to 13..7 and read bit 0..6
    ub = gInterpreterFile.read();
    ret = (ret<<7) | (ub & 0x7f);
  }
  if (ub&0x80) {
    // shift to 20..14 and read bit 0..6
    ub = gInterpreterFile.read();
    ret = (ret<<7) | (ub & 0x7f);
  }
  if (ub&0x80) {
    // shift to 27..21 and read bit 0..6
    ub = gInterpreterFile.read();
    ret = (ret<<7) | (ub & 0x7f);
  }
  if (ub&0x80) {
    // shift to 34..28 (some bits are lost) and read bit 0..6
    ub = gInterpreterFile.read();
    ret = (ret<<7) | (ub & 0x7f);
  }
  return ret;
}

// ---------------- functions and lookup

// 1:
void interpFirePattern(void*)
{
  int pattern = interpreterReadShort();
  inkFirePattern(pattern, 15);
}

// 2:
void interpFirePatternN(void*)
{
  int pattern = interpreterReadShort();
  int nDrops = interpreterReadShort();
  inkFirePattern(pattern, nDrops);
}

// 144:
void interpGotoX(void*)
{
  long x = interpreterReadLong();
  stepperGotoX(x);
}

// 145:
void interpMoveX(void*)
{
  long dx = interpreterReadLong();
  stepperMoveX(dx);
}

// 146:
void interpHomeX(void*)
{
  interpreterReadLong(); // max x
  stepperHomeX();
}

// 147:
void interpGotoY(void*)
{
  long y = interpreterReadLong();
  stepperGotoY(y);
}

// 148:
void interpMoveY(void*)
{
  long dy = interpreterReadLong();
  stepperMoveY(dy);
}

// 149:
void interpHomeY(void*)
{
  interpreterReadLong(); // max y
  stepperHomeY();
}

// 157: clean the roller on the squeegie
void interpCleanRoller(void*)
{
  motorRollerClean();
}

// 158: dz is int 100th mm
void interpSpread(void*)
{
  long dz = interpreterReadLong(); // z height in steps (no, currentli in mm (FIXME!))
  stepperMoveZ2(-dz*16);
  stepperSpreadLayer(dz);
}

// 159: number of layers in total
void interpNumLayers(void*)
{
  // long follows, but it is read in the interpreter loop
}

CallbackPtr interpreterCommandLUT[] = {
  0L, interpFirePattern, interpFirePatternN, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, // 0
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  // ---
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, // 128
  interpGotoX, interpMoveX, interpHomeX, interpGotoY, interpMoveY, interpHomeY, 0L, 0L, 
      0L, 0L, 0L, 0L, 0L, interpCleanRoller, interpSpread, interpNumLayers, // 144
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, // 160
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L,
  0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, 
  // 158 spread
};

void interpreterRunFromSD(const char *name)
{
  SD.begin(driveSelectPin);
  gInterpreterFile = SD.open(name);
  if (!gInterpreterFile) {
    firmwareError("", "Can't open file:", name);
    return;
  }
  long currentLayer = 1;
  long nLayers = 0;
  interpreterReadLong();
  interpreterReadLong();
  interpreterReadLong();
  interpreterReadLong();
  for ( ; gInterpreterFile.available(); ) {
    int cmd = interpreterReadShort();
    if (cmd==-1) break;
    if (cmd==158 || cmd==159) {
      char buf[24];
      if (cmd==158) currentLayer++;
      if (cmd==159) nLayers = interpreterReadLong();
      ltoa(currentLayer, buf, 10);
      int n = strlen(buf);
      buf[n] = '/';
      if (nLayers) {
        ltoa(nLayers, buf+n+1, 10);
      } else {
        buf[n+1] = '?';
        buf[n+2] = 0;
      }
      displayAt(3,8, buf);
      int k = keysScan();
      if (k==kKeyBack)
        break;
      if (k==kKeyUp) {
        displayAt(4,1, "          [cnt]  --+");
        for (;;) {
          delay(100);
          if (keysScan()==kKeyDown) 
            break;
        }
        displayAt(4,1, "[Brk][Pse]       --+");
      }
    }
    if (cmd*sizeof(CallbackPtr) < sizeof(interpreterCommandLUT)) {
      CallbackPtr fn = interpreterCommandLUT[cmd];
      if (fn) {
        (*fn)(0L);
      }
    }
  }  
  gInterpreterFile.close();
  SD.end();
}

void interpreterRunFromSDCB(void *name)
{
  displayScreen(
                "+-- Run from SD:    ",
                "|                   ",
                " Start layer        ",
                "[ \177 ][L--][L++][Go!]");
  displayAt(2, 3, (const char*)name);
  long startLayer = 0;
  for (;;) {
    delay(100);
    char buf[12];
    ltoa(startLayer, buf, 10);
    displayAt(3, 14, buf);
    displayAt(3, 14, "     ");
    switch (keysScan() | (gKeysDown&(kKeyUp|kKeyDown))) {
      case kKeyBack: return;
      case kKeyUp: startLayer--; break;
      case kKeyDown: startLayer++; break;
      case kKeyOK: goto cont1;
    }
  }
cont1:
  displayScreen(
                "+-- Run from SD:    ",
                "|                   ",
                " Layer ?/?         |",
                "[Brk][Pse]       --+");
  displayAt(2, 3, (const char*)name);
  interpreterRunFromSD((const char*)name);
}


// ================ firmware

const int kMenuTypeMask = 7;
const int kMenuStart = 1;
const int kMenuEnd = 2;
const int kMenuSubmenu = 0x10;
const int kMenuCall = 0x20;
const int kMenuKeyRepeat = 0x4000;

typedef struct MenuItem
{
  const char *label; // no more than 14 characters
  int flags;
  void *user_ptr;
  void *user_data;
}
MenuItem;

extern MenuItem gFirmwareMenuMain[];

extern MenuItem gFirmwareMenuFile[];
extern MenuItem gFirmwareMenuSpread[];
extern MenuItem gFirmwareMenuTest[];

extern MenuItem gFirmwareMenuTestXAxis[];
extern MenuItem gFirmwareMenuTestYAxis[];
extern MenuItem gFirmwareMenuTestZ1Axis[];
extern MenuItem gFirmwareMenuTestZ2Axis[];
extern MenuItem gFirmwareMenuTestRoller[];
extern MenuItem gFirmwareMenuTestInk[];

void firmwareHomeXCB(void*)
{
  stepperHomeX();
}

void firmwarePowerOffXCB(void*)
{
  stepperPowerX(0);
}

void firmwareMoveXCB(void *d)
{
  int dist = (int)d;
  int dist_lut[] = { 5000, 500, 50 };
  displayScreen(
                "Move X Axis",
                "X:",
                "                    ",
                "[ \177 ][<<<][>>>]     ");
  int dirty = 1;
  for (;;) {
    if (dirty) {
      char buf[12];
      ltoa(gStepperCurrentX, buf, 10);
      displayNAt(10, 2, 4, buf);
      dirty = 0;
    }
    switch (keysScan() | (gKeysDown&(kKeyUp|kKeyDown))) {
      case kKeyBack: return;
      case kKeyUp: stepperMoveX(-dist_lut[dist]); dirty = 1; break;
      case kKeyDown: stepperMoveX(dist_lut[dist]); dirty = 1; break;
    }
  }
}

void firmwareHomeYCB(void*)
{
  stepperHomeY();
}

void firmwarePowerOffYCB(void*)
{
  stepperPowerY(0);
}

void firmwareMoveYCB(void *d)
{
  int dist = (int)d;
  int dist_lut[] = { 5000, 500, 50 };
  displayScreen(
                "Move Y Axis",
                "Y:",
                "                    ",
                "[ \177 ][^^^][vvv]     ");
  int dirty = 1;
  for (;;) {
    if (dirty) {
      char buf[12];
      ltoa(gStepperCurrentY, buf, 10);
      displayNAt(10, 2, 4, buf);
      dirty = 0;
    }
    switch (keysScan() | (gKeysDown&(kKeyUp|kKeyDown))) {
      case kKeyBack: return;
      case kKeyUp: stepperMoveY(-dist_lut[dist]); dirty = 1; break;
      case kKeyDown: stepperMoveY(dist_lut[dist]); dirty = 1; break;
    }
  }
}

void firmwareHomeZ1CB(void*)
{
  stepperHomeZ1();
}

void firmwarePowerOffZ1CB(void*)
{
  stepperPowerZ1(0);
}

void firmwareMoveZ1CB(void *d)
{
  int dist = (int)d;
  int dist_lut[] = { 5000, 500, 50 };
  displayScreen(
                "Move Z1 Axis",
                "Z1:",
                "                    ",
                "[ \177 ][ v ][ ^ ]     ");
  int dirty = 1;
  for (;;) {
    if (dirty) {
      char buf[12];
      ltoa(gStepperCurrentZ1, buf, 10);
      displayNAt(10, 2, 5, buf);
      dirty = 0;
    }
    switch (keysScan() | (gKeysDown&(kKeyUp|kKeyDown))) {
      case kKeyBack: return;
      case kKeyUp: stepperMoveZ1(-dist_lut[dist]); dirty = 1; break;
      case kKeyDown: stepperMoveZ1(dist_lut[dist]); dirty = 1; break;
    }
  }
}

void firmwareHomeZ2CB(void*)
{
  stepperHomeZ2();
}

void firmwarePowerOffZ2CB(void*)
{
  stepperPowerZ2(0);
}

void firmwareMoveZ2CB(void *d)
{
  int dist = (int)d;
  int dist_lut[] = { 5000, 500, 50 };
  displayScreen(
                "Move Z2 Axis",
                "Z2:",
                "                    ",
                "[ \177 ][ v ][ ^ ]     ");
  int dirty = 1;
  for (;;) {
    if (dirty) {
      char buf[12];
      ltoa(gStepperCurrentZ2, buf, 10);
      displayNAt(10, 2, 5, buf);
      dirty = 0;
    }
    switch (keysScan() | (gKeysDown&(kKeyUp|kKeyDown))) {
      case kKeyBack: return;
      case kKeyUp: stepperMoveZ2(-dist_lut[dist]); dirty = 1; break;
      case kKeyDown: stepperMoveZ2(dist_lut[dist]); dirty = 1; break;
    }
  }
}

// Spread a new layer of powder without lowering the build piston.
// This function is used to fill the build chamber before a print.
// dz is in 100th mm
//
void firmwareSpreadLayerCB(void *d) {
  int dz = (int)d;
  stepperSpreadLayer(dz);
  stepperGotoY(21500-1000);        // ready to print
  stepperPowerOff(0x001f);         // in interactive mode, power everything off?
}

void firmwareMoveZ1UpCB(void *d) {
  stepperMoveZ1(100*16);
}

void firmwareMoveZ1DownCB(void *d) {
  stepperMoveZ1(-100*16);
}

void firmwareMoveZ2UpCB(void *d) {
  stepperMoveZ2(100*16);
}

void firmwareMoveZ2DownCB(void *d) {
  stepperMoveZ2(-100*16);
}

void firmwareRollerPowerOffCB(void*)
{
  stepperPowerOff(0x0010);
}

void firmwareRollerMoveCB(void *d)
{
  stepperPowerOn(0x0010);
  switch ((int)d) {
    case 0:
      motorRoller(20, stepperRDelay, 1);
      break;
    case 1:
      motorRoller(20, stepperRDelay*2, 1);
      break;
    case 3:
      motorRoller(20, stepperRDelay*2, 0);
      break;
    case 4:
      motorRoller(20, stepperRDelay, 0);
      break;
  }
}

void firmwareRollerCleanCB(void *d)
{
  motorRollerClean();
}


void firmwareHomeXYCB(void*)
{
  stepperHomeX();
  stepperHomeY();
}


//
// Print "Hello World!" at the current Y position
//
void firmwarePrintHelloCB(void *d)
{
  stepperGotoX(0);
  stepperGotoY(21500);
  stepperGotoX(1000);
  inkWrite("Hello, world!");
  stepperHomeX();
}

//
// Print a black line (fire all nozzles a few hundred times) to clean out all nozzles
//
void firmwarePrintBlackLineCB(void *d)
{
  stepperGotoX(0);
  stepperGotoY(21500);
  stepperGotoX(100);
  inkFirePattern(0xfff, 10, 283); // (10cm)
  stepperGotoX(100);
  stepperHomeX();
}

//
// Print a vertical bar, then fire every nozzle for 8mm or so, then print the end bar.
// This can be used to test if all nozzles fire.
//
void firmwarePrintNozzleTestCB(void *d)
{
  stepperHomeX();
  stepperGotoY(21500);
  stepperGotoX(100);
  inkFirePattern(0xfff, 10, 20);
  inkFirePattern(0x001, 10, 20);
  inkFirePattern(0x002, 10, 20);
  inkFirePattern(0x004, 10, 20);
  inkFirePattern(0x008, 10, 20);
  inkFirePattern(0x010, 10, 20);
  inkFirePattern(0x020, 10, 20);
  inkFirePattern(0x040, 10, 20);
  inkFirePattern(0x080, 10, 20);
  inkFirePattern(0x100, 10, 20);
  inkFirePattern(0x200, 10, 20);
  inkFirePattern(0x400, 10, 20);
  inkFirePattern(0x800, 10, 20);
  inkFirePattern(0xfff, 10, 20);
  stepperHomeX();
}

//
// Print multiple cubes at various saturations.
// Use this to find the crrect amount of binder for a given powder.
// prints 10 pellets: 8mm long, 4.2mm wide, 2mm thick
//
void firmwarePrintSaturationCB(void *d)
{
  static int lut[] = { 1, 2, 3, 4, 5, 7, 10, 15, 20 }; //, 11, 14, 17, 20 };
  int layer, track, sat;
  displayScreen(
                "+---",
                "| Saturation Test",
                "| Layer: ",
                "[brk]");
  for (layer=0; layer<25; layer++) {
    // display
    char buf[12];
    ltoa(layer, buf, 10);
    displayNAt(5, 3, 10, buf);
    stepperHomeX();
    // break?
    if (keysScan()==kKeyBack)
      goto bail;
    // spread
    if (layer!=0) {
      stepperMoveZ2(-10*16);
      stepperSpreadLayer(10);
    }
    stepperGotoY(22000);    
    for (track=0; track<4; track++) {
      if (track!=0) stepperMoveY(9); // quarter nozzle vertical
      stepperGotoX(1000+60*36);
      for (sat=0; sat<sizeof(lut)/sizeof(int); sat++) {
        inkFirePattern(0xfff, lut[sat], 22); // 8mm wide
        stepperMoveX(8*36); // 8mm break
      }
    }
  }
bail:
  stepperHomeX();
  stepperHomeY();
  stepperPowerOff(0xff);
}

void firmwarePrintCB(void *d)
{
  int what = (int)d;
  int i, j, k;
  static const int lut[] = { 1, 5, 8, 10, 15, 35 };
  static const int lut2[] = { 1, 3, 7, 12, 15 };
  switch (what) {
    case 4: // ratio bars
      for (j=0; j<32; j++) {
        stepperMoveZ2(-25*16);
        stepperSpreadLayer(25);
        stepperHomeX();
        stepperGotoY(21500);
        for (i=0; i<6; i++) {
          int nn = lut[i];
          int adv = (35/nn);
          for (k=0; k<nn; k++) {
            stepperGotoX(100);
            inkFirePattern(0xfff, 3, 150);
            stepperMoveY(adv);
          }
          stepperGotoX(100);
          stepperMoveY(428*3/2);
        }
      }
      stepperHomeX();
      stepperGotoY(21500-2*428);
      break;
    case 5: // cubed cups  (0.1mm layered 1x1x1cm^3 cubes at different ink saturation)
      for (j=0; j<50; j++) {
        stepperHomeX();
        stepperGotoY(27500);
        for (i=0; i<4; i++) {
          for (k=0; k<5; k++) {
            int nn = lut2[k];
            stepperGotoX(500+k*20*135); // 135 steps per mm
            inkFirePattern(0xfff, nn, 20); //   1 cm wide
            inkFirePattern(0x0f0, nn, 10); // 0.5 cm wide nose
          }
          stepperMoveY(9);
        }
        stepperMoveZ2(-10*16);
        stepperSpreadLayer(10);
      }
      stepperHomeX();
      stepperHomeY();
      stepperPowerOff(0x1f);
      break;
  }
}

void firmwarePowerOffXYCB(void*)
{
  stepperPowerX(0);
  stepperPowerY(0);
}

void firmwareMenuAboutCB(void*)
{
  displayAboutScreen();
  while (keysScan()==0) { } // press any key
}

void firmwareError(const char *line1, const char *line2, const char *line3)
{
  displayScreen("   *** ERROR ***    ", line1, line2, line3);
  while (keysScan()==0) { } // press any key
}

MenuItem *gSDFileMenu = 0;
void firmwareMenu(const MenuItem *m);

void firmwareFillWithDirCB(void*)
{
  int i, nf;
  if (!gSDFileMenu) {
    gSDFileMenu = (MenuItem*)calloc(100, 1);
    gSDFileMenu[0].label = 0;
    gSDFileMenu[0].flags = kMenuStart;
    gSDFileMenu[0].user_ptr = gFirmwareMenuMain;
    gSDFileMenu[0].user_data = 0;
  }
  SD.begin(driveSelectPin);
  File dir = SD.open("/");
  if (!dir) {
    firmwareError("", "Can't find SD card");
    return;
  }
  dir.rewindDirectory();
  for (i=1, nf=1; nf<98; i++) {
    File f = dir.openNextFile();
    if (!f) break;
    if (f.isDirectory()) {
    } else if (strstr(f.name(), ".3DP")) {
      if (!gSDFileMenu[nf].label) {
        gSDFileMenu[nf].label = (const char*)malloc(13);
      }
      strcpy((char*)gSDFileMenu[nf].label, f.name());
      gSDFileMenu[nf].flags = kMenuCall;
      gSDFileMenu[nf].user_ptr = (void*)interpreterRunFromSDCB;
      gSDFileMenu[nf].user_data = (void*)gSDFileMenu[nf].label;
      nf++;
    }
    f.close();
  }
  if (nf==1) {
    if (!gSDFileMenu[nf].label) {
      gSDFileMenu[nf].label = (const char*)malloc(13);
    }
    strcpy((char*)gSDFileMenu[nf].label, "<empty>");
    gSDFileMenu[nf].flags = 0;
    gSDFileMenu[nf].user_ptr = 0;
    gSDFileMenu[nf].user_data = 0;
  }
  if (gSDFileMenu[nf].label) {
    free((void*)gSDFileMenu[nf].label); // avoid memory leak
    gSDFileMenu[nf].label = 0;
  }
  gSDFileMenu[nf].flags = kMenuEnd;
  gSDFileMenu[nf].user_ptr = 0;
  gSDFileMenu[nf].user_data = 0;
  dir.close();
  SD.end();
  firmwareMenu(gSDFileMenu);
}

void firmwareAllMotorsOff(void*)
{
  stepperPowerOff(0xff);
}

MenuItem gFirmwareMenuMain[] =
{
  { 0, kMenuStart },
  { "Print from SD", kMenuCall, (void*)firmwareFillWithDirCB },
  { "Spread", kMenuSubmenu, gFirmwareMenuSpread },
  { "Test", kMenuSubmenu, gFirmwareMenuTest },
  { "About", kMenuCall, (void*)firmwareMenuAboutCB },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuSpread[] =
{
  { 0, kMenuStart, gFirmwareMenuMain, (void*)firmwareAllMotorsOff },
  { "Spread 1.0mm", kMenuCall, (void*)firmwareSpreadLayerCB, (void*)100 },
  { "Spread 0.5mm", kMenuCall, (void*)firmwareSpreadLayerCB, (void*)50 },
  { "Spread 0.25mm", kMenuCall, (void*)firmwareSpreadLayerCB, (void*)25  },
  { "Piston 1 Up", kMenuCall | kMenuKeyRepeat, (void*)firmwareMoveZ1UpCB },
  { "Piston 1 Down", kMenuCall | kMenuKeyRepeat, (void*)firmwareMoveZ1DownCB },
  { "Piston 2 Up", kMenuCall | kMenuKeyRepeat, (void*)firmwareMoveZ2UpCB },
  { "Piston 2 Down", kMenuCall | kMenuKeyRepeat, (void*)firmwareMoveZ2DownCB },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuTest[] =
{
  { 0, kMenuStart, gFirmwareMenuMain, (void*)firmwareAllMotorsOff },
  { "Power Off All", kMenuCall, (void*)firmwareAllMotorsOff },
  { "X Axis", kMenuSubmenu, gFirmwareMenuTestXAxis },
  { "Y Axis", kMenuSubmenu, gFirmwareMenuTestYAxis },
  { "Z1 Hopper", kMenuSubmenu, gFirmwareMenuTestZ1Axis },
  { "Z2 Hopper", kMenuSubmenu, gFirmwareMenuTestZ2Axis },
  { "Roller", kMenuSubmenu, gFirmwareMenuTestRoller },
  //{ "(End Stops)" },
  { "Ink Cartridge", kMenuSubmenu, gFirmwareMenuTestInk },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuTestXAxis[] =
{
  { 0, kMenuStart, gFirmwareMenuTest, (void*)firmwareAllMotorsOff },
  //  { "Move 5cm", kMenuCall, (void*)firmwareMoveXCB, (void*)0 },
  { "Move 1cm", kMenuCall, (void*)firmwareMoveXCB, (void*)1 },
  //  { "Move 1mm", kMenuCall, (void*)firmwareMoveXCB, (void*)2 },
  { "Home", kMenuCall, (void*)firmwareHomeXCB },
  { "Power Off", kMenuCall, (void*)firmwarePowerOffXCB },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuTestYAxis[] =
{
  { 0, kMenuStart, gFirmwareMenuTest, (void*)firmwareAllMotorsOff },
  //  { "Move 5cm", kMenuCall, (void*)firmwareMoveYCB, (void*)0 },
  { "Move 1cm", kMenuCall, (void*)firmwareMoveYCB, (void*)1 },
  //  { "Move 1mm", kMenuCall, (void*)firmwareMoveYCB, (void*)2 },
  { "Home", kMenuCall, (void*)firmwareHomeYCB },
  { "Power Off", kMenuCall, (void*)firmwarePowerOffYCB },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuTestZ1Axis[] =
{
  { 0, kMenuStart, gFirmwareMenuTest, (void*)firmwareAllMotorsOff },
  //  { "Move 5cm", kMenuCall, (void*)firmwareMoveZ1CB, (void*)0 },
  { "Move 1cm", kMenuCall, (void*)firmwareMoveZ1CB, (void*)1 },
  //  { "Move 1mm", kMenuCall, (void*)firmwareMoveZ1CB, (void*)2 },
  //  { "Home", kMenuCall, (void*)firmwareHomeZ1CB },
  { "Power Off", kMenuCall, (void*)firmwarePowerOffZ1CB },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuTestZ2Axis[] =
{
  { 0, kMenuStart, gFirmwareMenuTest, (void*)firmwareAllMotorsOff },
  //  { "Move 5cm", kMenuCall, (void*)firmwareMoveZ2CB, (void*)0 },
  { "Move 1cm", kMenuCall, (void*)firmwareMoveZ2CB, (void*)1 },
  //  { "Move 1mm", kMenuCall, (void*)firmwareMoveZ2CB, (void*)2 },
  //  { "Home", kMenuCall, (void*)firmwareHomeZ2CB },
  { "Power Off", kMenuCall, (void*)firmwarePowerOffZ2CB },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuTestRoller[] =
{
  { 0, kMenuStart, gFirmwareMenuTest, (void*)firmwareAllMotorsOff },
  { "Forward", kMenuCall | kMenuKeyRepeat, (void*)firmwareRollerMoveCB, (void*)0 },
  { "Back", kMenuCall | kMenuKeyRepeat, (void*)firmwareRollerMoveCB, (void*)4 },
  { "Clean", kMenuCall | kMenuKeyRepeat, (void*)firmwareRollerCleanCB, (void*)0 },
  { 0, kMenuEnd }
};

MenuItem gFirmwareMenuTestInk[] =
{
  { 0, kMenuStart, gFirmwareMenuTest, (void*)firmwareAllMotorsOff },
  { "Home Axes", kMenuCall, (void*)firmwareHomeXYCB },
  { "Hello World!", kMenuCall, (void*)firmwarePrintHelloCB },
  { "Black Line", kMenuCall, (void*)firmwarePrintBlackLineCB },
  { "Nozzle Test", kMenuCall, (void*)firmwarePrintNozzleTestCB },
  { "Saturation 0.1", kMenuCall, (void*)firmwarePrintSaturationCB, (void*)4 },
//  { "Cubed Cups", kMenuCall, (void*)firmwarePrintCB, (void*)5 },
  { "Power Off", kMenuCall, (void*)firmwarePowerOffXYCB },
  { 0, kMenuEnd }
};


const MenuItem *gCurrentMenu = 0L;

// ---------------- menus

void firmwareMenu(const MenuItem *m)
{
  displayScreen(
                "                   H",       // home symbol
                ">>              <<  ",       // arrows?
                "                    ",       // play symbol
                "[ \177 ][ ^ ][ v ][ OK]");  // check box for OK?
  gCurrentMenu = m;
}

void firmwareDisplayMenuItemAt(int row, int col, const MenuItem *m) {
  if (m->label) {
    displayNAt(14, row, col, m->label);
  } else {
    if ((m->flags&kMenuTypeMask)==kMenuStart) {
      displayNAt(14, row, col, "\242");
    } else if ((m->flags&kMenuTypeMask)==kMenuEnd) {
      displayNAt(14, row, col, "             \243");
    } else {
      displayNAt(14, row, col, "   \245\245\245");
    }
  }
}

const MenuItem *firmwareMenuLast(const MenuItem *m)
{
  // special case: empty menu
  if ((m->flags&kMenuTypeMask)==kMenuEnd) return m;
  for (;;) {
    m++;
    if ((m->flags&kMenuTypeMask)==kMenuEnd) return m-1;
  }
}

const MenuItem *firmwareMenuFirst(const MenuItem *m)
{
  for (;;) {
    if ((m->flags&kMenuTypeMask)==kMenuStart) return m;
    m--;
  }
}

int firmwareHandleMenu()
{
  int dirty = 1, empty = 0;
  
  for (;;) {
    // print menu
    if (dirty) {
      if (!gCurrentMenu) return 0;
      if ((gCurrentMenu->flags&kMenuTypeMask)==kMenuStart) gCurrentMenu++;
      if ((gCurrentMenu->flags&kMenuTypeMask)==kMenuEnd) {
        empty = 1;
        displayAt(1, 3, "--------------");
        displayAt(2, 3, "--  empty   --");
        displayAt(3, 3, "--------------");
      } else {
        empty = 0;
        firmwareDisplayMenuItemAt(1, 3, gCurrentMenu-1);
        firmwareDisplayMenuItemAt(2, 3, gCurrentMenu);
        firmwareDisplayMenuItemAt(3, 3, gCurrentMenu+1);
      }
    }
    // wait for button
    //displayDebug("Button");
    dirty = 0;
    delay(10);
    int keys = keysScan();
    if (gCurrentMenu->flags & kMenuKeyRepeat )
      keys |= (gKeysDown&kKeyOK);
    switch (keys) {
      case kKeyHome:
        gCurrentMenu = firmwareMenuFirst(gCurrentMenu);
        while (gCurrentMenu->user_ptr) {
          if (gCurrentMenu->user_data) {
            CallbackPtr fn = (CallbackPtr)gCurrentMenu->user_data;
            (*fn)((void*)gCurrentMenu);
          }
          gCurrentMenu = (const MenuItem*)gCurrentMenu->user_ptr;
          gCurrentMenu = firmwareMenuFirst(gCurrentMenu);
        }
        beeperBleep();
        dirty = 1;
        break;
      case kKeyPlay:
        break;
      case kKeyBack:
        gCurrentMenu = firmwareMenuFirst(gCurrentMenu);
        if (gCurrentMenu->user_ptr) {
          if (gCurrentMenu->user_data) {
            CallbackPtr fn = (CallbackPtr)gCurrentMenu->user_data;
            (*fn)((void*)gCurrentMenu);
          }
          gCurrentMenu = (const MenuItem*)gCurrentMenu->user_ptr;
          beeperBlip();
        } else {
          beeperBleep();
        }
        dirty = 1;
        break;
      case kKeyUp:
        if (!empty) {
          gCurrentMenu--;
          if ((gCurrentMenu->flags&kMenuTypeMask)==kMenuStart) {
            gCurrentMenu = firmwareMenuLast(gCurrentMenu);
            beeperBleep();
          } else {
            beeperBlip();
          }
          dirty = 1;
        }
        break;
      case kKeyDown:
        if (!empty) {
          gCurrentMenu++;
          if ((gCurrentMenu->flags&kMenuTypeMask)==kMenuEnd) {
            gCurrentMenu = firmwareMenuFirst(gCurrentMenu);
            beeperBleep();
          } else {
            beeperBlip();
          }
          dirty = 1;
        }
        break;
      case kKeyOK:
        if (!empty) {
          if ((gCurrentMenu->flags&kMenuSubmenu) && gCurrentMenu->user_ptr) {
            gCurrentMenu = (const MenuItem*)gCurrentMenu->user_ptr;
            beeperBlip();
          } else if ((gCurrentMenu->flags&kMenuCall) && gCurrentMenu->user_ptr) {
            beeperBlip();
            CallbackPtr fn = (CallbackPtr)gCurrentMenu->user_ptr;
            (*fn)(gCurrentMenu->user_data);
            beeperBlip();
            firmwareMenu(gCurrentMenu);
          } else {
            beeperBleep();
          }
          dirty = 1;
        }
        break;
    }
  }
  return 0;
}

int firmwareLoop()
{
  firmwareMenu(gFirmwareMenuMain);
  for (;;) {
    firmwareHandleMenu();
  }
  return 1;
}


// ================ setup and main


// ---------------- setup

void setup() {
  setupBeeper();
  setupDisplay();
  setupKeys();
  setupSteppers();
  setupMotors();
  setupInk();  
  setupDrive();  
}


// ---------------- main


void loop() {
  firmwareLoop();
}





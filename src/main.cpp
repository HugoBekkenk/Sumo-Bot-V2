#include <Arduino.h>
#include <ESP32Servo.h>

#ifndef PHONE_CONTROL
#include <esp32-hal-alloc-bt-classic-mem.h>
#include <PS4Controller.h>
#include <esp_err.h>
#include <esp_mac.h>
#else
#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE
#include <DabbleESP32.h>
#endif

const int leftServoPin = 25;
const int rightServoPin = 26;
const int leftPositionServoPin = 27;
const int rightPositionServoPin = 14;
const int positionServoMinimum = 0;
const int positionServoMaximum = 180;

// Calibrate these so each servo is stationary at its corresponding value.
const int leftStopUs = 1542; //1533; //1530;
const int rightStopUs = 1504; //1503 / 1503.9
const int maximumDriveOffsetUs = 500;

const int triggerDeadzone = 10;
const int triggerMaximum = 255;

// PS4 left stick X values are normally about -127 full left to +127 full right.
const int stickDeadzone = 15;
const int stickMaximum = 127;
const bool invertSteering = false;

const int updateDelayMs = 20;
const int debugOutputIntervalMs = 250;

Servo leftServo;
Servo rightServo;
Servo leftPositionServo;
Servo rightPositionServo;
unsigned long lastDebugOutputMs = 0;

#ifdef PHONE_CONTROL
int phonePositionAngle = 90;
bool phoneInputReceived = false;
#endif

// 20:9B:A9:8B:DE:D2 

// 9:8B:DE:D2

#ifndef PHONE_CONTROL
void printBluetoothAddress() {
  uint8_t address[6];
  esp_err_t result = esp_read_mac(address, ESP_MAC_BT);
  if (result != ESP_OK) {
    Serial.printf("Could not read ESP32 Bluetooth address: %s\n",
                  esp_err_to_name(result));
    return;
  }

  Serial.printf(
    "ESP32 Bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X\n",
    address[0], address[1], address[2],
    address[3], address[4], address[5]
  );
}
#endif

int reducedSpeedBySteering(int speed, float scale) {
  return (int)round((float)speed * scale);
}

void drive(int speed, int steeringX = 0) {
  speed = constrain(speed, -maximumDriveOffsetUs, maximumDriveOffsetUs);

  if (invertSteering) {
    steeringX = -steeringX;
  }
  steeringX = constrain(steeringX, -stickMaximum, stickMaximum);

  float leftScale = 1.0f;
  float rightScale = 1.0f;

  if (steeringX <= -stickDeadzone) {
    // Stick left: gradually reduce the left servo speed.
    // Just outside deadzone = almost full speed, full left = stopped.
    float turnAmount = (float)(-steeringX - stickDeadzone) /
                       (stickMaximum - stickDeadzone);
    turnAmount = constrain(turnAmount, 0.0f, 1.0f);
    leftScale = 1.0f - turnAmount;
  } else if (steeringX >= stickDeadzone) {
    // Stick right: gradually reduce the right servo speed.
    // Just outside deadzone = almost full speed, full right = stopped.
    float turnAmount = (float)(steeringX - stickDeadzone) /
                       (stickMaximum - stickDeadzone);
    turnAmount = constrain(turnAmount, 0.0f, 1.0f);
    rightScale = 1.0f - turnAmount;
  }

  int leftSpeed = reducedSpeedBySteering(speed, leftScale);
  int rightSpeed = reducedSpeedBySteering(speed, rightScale);

  leftServo.writeMicroseconds(leftStopUs + leftSpeed);
  rightServo.writeMicroseconds(rightStopUs - rightSpeed);
}

void positionServos(int stickX) {
  stickX = constrain(stickX, -stickMaximum, stickMaximum);

  int leftAngle = map(stickX, -stickMaximum, stickMaximum,
                      positionServoMinimum, positionServoMaximum);
  leftPositionServo.write(leftAngle);
  rightPositionServo.write(positionServoMinimum + positionServoMaximum -
                           leftAngle);
}

#ifdef PHONE_CONTROL
const float phoneSteeringAxisMaximum = 6.0f;

void positionServosAtAngle(int leftAngle) {
  leftAngle = constrain(leftAngle, positionServoMinimum,
                        positionServoMaximum);
  int rightAngle = positionServoMinimum + positionServoMaximum - leftAngle;

  leftPositionServo.write(leftAngle);
  rightPositionServo.write(rightAngle);
}

int phoneAxisToSteering(float axis) {
  axis = constrain(axis, -phoneSteeringAxisMaximum,
                   phoneSteeringAxisMaximum);
  return (int)round(axis * stickMaximum / phoneSteeringAxisMaximum);
}
#else
void onConnect() {
  Serial.println("PS4 controller connected.");
}

void onDisconnect() {
  drive(0);
  positionServos(0);
  Serial.println("PS4 controller disconnected. Servos stopped.");
}
#endif

int triggerToSpeed(int triggerValue) {
  if (triggerValue <= triggerDeadzone) {
    return 0;
  }

  return map(triggerValue, triggerDeadzone, triggerMaximum, 0,
             maximumDriveOffsetUs);
}

void setup() {
  Serial.begin(115200);
#ifndef PHONE_CONTROL
  printBluetoothAddress();
#endif

  leftServo.setPeriodHertz(50);
  rightServo.setPeriodHertz(50);
  leftPositionServo.setPeriodHertz(50);
  rightPositionServo.setPeriodHertz(50);
  leftServo.attach(leftServoPin, 500, 2500);
  rightServo.attach(rightServoPin, 500, 2500);
  leftPositionServo.attach(leftPositionServoPin, 500, 2500);
  rightPositionServo.attach(rightPositionServoPin, 500, 2500);
  drive(0);
  positionServos(0);

#ifdef PHONE_CONTROL
  Dabble.begin("SumoBot");
  Serial.println("Dabble BLE initialized. GamePad X: steering.");
  Serial.println("Hold Square: forward. Hold Circle: reverse. Release to stop.");
  Serial.println("Triangle/Cross: move position servos left/right.");
#else
  PS4.attachOnConnect(onConnect);
  PS4.attachOnDisconnect(onDisconnect);
  if (!PS4.begin()) {
    Serial.println("PS4 Bluetooth initialization failed.");
  } else {
    Serial.println("PS4 Bluetooth initialized.");
  }

  Serial.println("R2: forward. L2: reverse. Left stick X: steering. Right stick X: position servos.");
#endif
}

void loop() {
#ifdef PHONE_CONTROL
  Dabble.processInput();
  if (!Dabble.isAppConnected()) {
    drive(0);
    positionServos(0);
    phonePositionAngle = 90;

    if (millis() - lastDebugOutputMs >= 1000) {
      Serial.println(
        "No Dabble input received. Open GamePad and move the joystick."
      );
      lastDebugOutputMs = millis();
    }

    delay(100);
    return;
  }

  if (!phoneInputReceived) {
    Serial.println("Dabble input received.");
    phoneInputReceived = true;
  }

  float steeringAxis = GamePad.getXaxisData();
  int steeringX = phoneAxisToSteering(steeringAxis);

  bool dpadUp = GamePad.isUpPressed();
  bool dpadDown = GamePad.isDownPressed();
  bool dpadLeft = GamePad.isLeftPressed();
  bool dpadRight = GamePad.isRightPressed();
  bool squarePressed = GamePad.isSquarePressed();
  bool circlePressed = GamePad.isCirclePressed();
  bool crossPressed = GamePad.isCrossPressed();
  bool trianglePressed = GamePad.isTrianglePressed();
  bool startPressed = GamePad.isStartPressed();
  bool selectPressed = GamePad.isSelectPressed();

  int driveSpeed = 0;
  if (squarePressed != circlePressed) {
    driveSpeed = squarePressed ? maximumDriveOffsetUs
                               : -maximumDriveOffsetUs;
  }
  drive(driveSpeed, steeringX);

  if (trianglePressed) {
    phonePositionAngle--;
  }
  if (crossPressed) {
    phonePositionAngle++;
  }
  positionServosAtAngle(phonePositionAngle);

  if (millis() - lastDebugOutputMs >= debugOutputIntervalMs) {
    Serial.printf(
      "GamePad steerX:%+.2f | D-pad U%d D%d L%d R%d | "
      "buttons Sq%d Ci%d Cross%d Tri%d St%d Se%d | "
      "drive:%4d steer:%4d pos:%3d\n",
      steeringAxis,
      dpadUp, dpadDown, dpadLeft, dpadRight,
      squarePressed, circlePressed, crossPressed, trianglePressed,
      startPressed, selectPressed,
      driveSpeed, steeringX, phonePositionAngle
    );
    lastDebugOutputMs = millis();
  }

  delay(updateDelayMs);
#else
  if (!PS4.isConnected()) {
    drive(0);
    delay(100);
    return;
  }

  int r2Value = PS4.R2Value();
  int l2Value = PS4.L2Value();
  int steeringX = PS4.LStickX();
  int positionStickX = PS4.RStickX();

  int forwardSpeed = triggerToSpeed(r2Value);
  int reverseSpeed = triggerToSpeed(l2Value);
  int driveSpeed = constrain(
    forwardSpeed - reverseSpeed,
    -maximumDriveOffsetUs,
    maximumDriveOffsetUs
  );

  drive(driveSpeed, steeringX);
  positionServos(positionStickX);

  int debugSteeringX = invertSteering ? -steeringX : steeringX;
  debugSteeringX = constrain(debugSteeringX, -stickMaximum, stickMaximum);

  float leftScale = 1.0f;
  float rightScale = 1.0f;
  if (debugSteeringX <= -stickDeadzone) {
    float turnAmount = (float)(-debugSteeringX - stickDeadzone) /
                       (stickMaximum - stickDeadzone);
    turnAmount = constrain(turnAmount, 0.0f, 1.0f);
    leftScale = 1.0f - turnAmount;
  } else if (debugSteeringX >= stickDeadzone) {
    float turnAmount = (float)(debugSteeringX - stickDeadzone) /
                       (stickMaximum - stickDeadzone);
    turnAmount = constrain(turnAmount, 0.0f, 1.0f);
    rightScale = 1.0f - turnAmount;
  }

  int leftSpeed = reducedSpeedBySteering(driveSpeed, leftScale);
  int rightSpeed = reducedSpeedBySteering(driveSpeed, rightScale);

  if (millis() - lastDebugOutputMs >= debugOutputIntervalMs) {
    Serial.printf(
      "R2 %s:%3d | L2 %s:%3d | drive:%4d | steerX:%4d | speed L:%4d R:%4d | pulse L:%4d R:%4d us\n",
      r2Value > triggerDeadzone ? "pressed " : "released",
      r2Value,
      l2Value > triggerDeadzone ? "pressed " : "released",
      l2Value,
      driveSpeed,
      steeringX,
      leftSpeed,
      rightSpeed,
      leftStopUs + leftSpeed,
      rightStopUs - rightSpeed
    );
    lastDebugOutputMs = millis();
  }

  delay(updateDelayMs);
#endif
}

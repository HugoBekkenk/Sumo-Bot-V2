#include <Arduino.h>
#include <esp32-hal-alloc-bt-classic-mem.h>
#include <ESP32Servo.h>
#include <PS4Controller.h>
#include <esp_err.h>
#include <esp_mac.h>

const int leftServoPin = 25;
const int rightServoPin = 26;

// Calibrate these so each servo is stationary at its corresponding value.
const int leftStopUs = 1530;
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
unsigned long lastDebugOutputMs = 0;

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

void onConnect() {
  Serial.println("PS4 controller connected.");
}

void onDisconnect() {
  drive(0);
  Serial.println("PS4 controller disconnected. Servos stopped.");
}

int triggerToSpeed(int triggerValue) {
  if (triggerValue <= triggerDeadzone) {
    return 0;
  }

  return map(triggerValue, triggerDeadzone, triggerMaximum, 0,
             maximumDriveOffsetUs);
}

void setup() {
  Serial.begin(115200);
  printBluetoothAddress();

  leftServo.setPeriodHertz(50);
  rightServo.setPeriodHertz(50);
  leftServo.attach(leftServoPin, 500, 2500);
  rightServo.attach(rightServoPin, 500, 2500);
  drive(0);

  PS4.attachOnConnect(onConnect);
  PS4.attachOnDisconnect(onDisconnect);
  if (!PS4.begin()) {
    Serial.println("PS4 Bluetooth initialization failed.");
  } else {
    Serial.println("PS4 Bluetooth initialized.");
  }

  Serial.println("R2: forward. L2: reverse. Left stick X: steering. Release both triggers to stop.");
}

void loop() {
  if (!PS4.isConnected()) {
    drive(0);
    delay(100);
    return;
  }

  int r2Value = PS4.R2Value();
  int l2Value = PS4.L2Value();
  int steeringX = PS4.LStickX();

  int forwardSpeed = triggerToSpeed(r2Value);
  int reverseSpeed = triggerToSpeed(l2Value);
  int driveSpeed = constrain(
    forwardSpeed - reverseSpeed,
    -maximumDriveOffsetUs,
    maximumDriveOffsetUs
  );

  drive(driveSpeed, steeringX);

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
}

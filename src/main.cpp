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

void drive(int speed) {
  speed = constrain(speed, -maximumDriveOffsetUs, maximumDriveOffsetUs);
  leftServo.writeMicroseconds(leftStopUs + speed);
  rightServo.writeMicroseconds(rightStopUs - speed);
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

  Serial.println("R2: forward. L2: reverse. Release both to stop.");
}

void loop() {
  if (!PS4.isConnected()) {
    drive(0);
    delay(100);
    return;
  }

  int r2Value = PS4.R2Value();
  int l2Value = PS4.L2Value();
  int forwardSpeed = triggerToSpeed(r2Value);
  int reverseSpeed = triggerToSpeed(l2Value);
  int driveSpeed = constrain(
    forwardSpeed - reverseSpeed,
    -maximumDriveOffsetUs,
    maximumDriveOffsetUs
  );
  drive(driveSpeed);

  if (millis() - lastDebugOutputMs >= debugOutputIntervalMs) {
    Serial.printf(
      "R2 %s:%3d | L2 %s:%3d | drive:%4d | pulse L:%4d R:%4d us\n",
      r2Value > triggerDeadzone ? "pressed " : "released",
      r2Value,
      l2Value > triggerDeadzone ? "pressed " : "released",
      l2Value,
      driveSpeed,
      leftStopUs + driveSpeed,
      rightStopUs - driveSpeed
    );
    lastDebugOutputMs = millis();
  }

  delay(updateDelayMs);
}

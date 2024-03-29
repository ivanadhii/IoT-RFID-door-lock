#define BLYNK_TEMPLATE_ID           "blynk tmp id"
#define BLYNK_TEMPLATE_NAME         "blynk tmp name"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define BLYNK_PRINT Serial

#define APP_DEBUG

#define USE_NODE_MCU_BOARD

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include "BlynkEdgent.h"

#define RST_PIN     D3  // Reset pin for MFRC522
#define SS_PIN      D8  // SS pin for MFRC522
#define MOSFET_PIN  D4  // Pin MOSFET IRFZ44N
#define BUTTON_PIN  D2  // Digital pin for Blynk button
#define BUTTON_PIN2 D0  // Digital pin for Physical Button
#define PIR_PIN     A0  // Analog pin for PIR sensor
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Initialize MFRC522
byte validUIDs[][4] = {
  {0x12, 0x79, 0xC0, 0x1B},   // UID for blue tag
  {0xB0, 0x7F, 0x4E, 0x21},   // UID for empty card
  {0x04, 0x4A, 0x5A, 0x7A},   // UID for Ivan Key1
  {0xA7, 0x05, 0x73, 0x38}    // UID for Ivan Key2
};

int sensorValue;
BlynkTimer timer;
bool isPhyButtonPressed = false;
bool isLocked = true;
bool unlockInProgress = false;
unsigned long unlockStartTime = 0;
const unsigned long unlockDuration = 5000; // Duration to keep door unlocked (in milliseconds)

char auth[] = "***";  // Blynk authentication token
char ssid[] = "***";  // WiFi SSID
char pass[] = "***";  // WiFi password

void setup()
{
  Serial.begin(115200);
  delay(100);

  SPI.begin();         // Initialize SPI bus
  mfrc522.PCD_Init();  // Initialize MFRC522

  pinMode(MOSFET_PIN, OUTPUT);  // Set MOSFET pin as output
  digitalWrite(MOSFET_PIN, LOW);  // Turn off MOSFET during startup

  pinMode(PIR_PIN, INPUT);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);  // Set physical button pin as input

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // Initialize SSD1306 module

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);  // Set text size to 2 (largest)
  display.setCursor(0, 0);
  display.display();

  Serial.println("Doorlock ready!");
  Serial.println("Scan your card to unlock the door.");
  Blynk.begin(auth, ssid, pass);  // Connect to Blynk server

  BlynkEdgent.begin();
}

void loop() {
  BlynkEdgent.run();

  int currentHour = hour();

  if (currentHour ==20) {
    Blynk.logEvent("PIR Sensor Activated!");
  }

  if (currentHour ==6) {
    Blynk.logEvent("PIR Sensor Deactivated!");
  }

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = getUID();
    Serial.print("UID: ");
    Serial.println(uid);

    if (checkUID(uid)) {
      if (!unlockInProgress && isLocked) {
        openDoor();
        unlockInProgress = true;
        unlockStartTime = millis();
        displayStatus("OPEN");
        Blynk.virtualWrite(V1, "UNLOCKED");
      }
      else if (unlockInProgress && !isLocked) {
        closeDoor();
        unlockInProgress = false;
        displayStatus("LOCKED");
        Blynk.virtualWrite(V1, "LOCKED");
      }
    }
    mfrc522.PICC_HaltA();  // Stop communication with the card
  }

  phySW();

  if (unlockInProgress && millis() - unlockStartTime >= unlockDuration) {
    closeDoor();
    unlockInProgress = false;
    displayStatus("LOCKED");
    Blynk.virtualWrite(V1, "LOCKED");
  }
}

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(V3) {
  TimeInputParam t(param);
  if (t.hasStartTime()) {
    int hour = t.getStartHour();
    int min = t.getStartMinute();
    int sec = t.getStartSecond();

    setTime(hour + 7, min, sec, day(), month(), year());
  }
}

BLYNK_WRITE(V2) {
  if (param.asInt()==1) {
      openDoor();
      displayStatus("OPEN");
      Blynk.virtualWrite(V1, "UNLOCKED");
      delay(5000);
      closeDoor();
      }
        else{
        closeDoor();
        unlockInProgress = false;
        displayStatus("LOCKED");
        Blynk.virtualWrite(V1, "LOCKED");
      }
}

BLYNK_WRITE(V4) {
  if (param.asInt()==1) {
    checkPIR();
    Blynk.virtualWrite(V5, "ACTIVATED");
  }
    else{
      Blynk.virtualWrite(V5, "DEACTIVATED");
    }
  }

void openDoor() {
  digitalWrite(MOSFET_PIN, HIGH);
  Serial.println("Door opened");
}

void closeDoor() {
  digitalWrite(MOSFET_PIN, LOW);
  Serial.println("Door locked");
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
    uid.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  return uid;
}

bool checkUID(String uid) {
  for (byte i = 0; i < sizeof(validUIDs) / sizeof(validUIDs[0]); i++) {
    String validUID = "";
    for (byte j = 0; j < sizeof(validUIDs[i]); j++) {
      validUID.concat(String(validUIDs[i][j] < 0x10 ? "0" : ""));
      validUID.concat(String(validUIDs[i][j], HEX));
    }
    if (uid == validUID) {
      return true;
    }
  }
  return false;
}

void displayStatus(String status) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);  // Set text size to 3 (largest)
  display.setTextWrap(false);
  int textWidth = status.length() * 14;
  int xPos = (SCREEN_WIDTH - textWidth) / 2;
  int yPos = (SCREEN_HEIGHT - 24) / 2;
  display.setCursor(xPos, yPos);
  display.println(status);
  display.display();
}

void phySW() {
  if (digitalRead(BUTTON_PIN2) == LOW) {
    if (!isPhyButtonPressed) {
      isPhyButtonPressed = true;
      if (!unlockInProgress && isLocked) {
        openDoor();
        unlockInProgress = true;
        unlockStartTime = millis();
        displayStatus("OPEN");
        Blynk.virtualWrite(V1, "UNLOCKED");
      }
      else if (unlockInProgress && !isLocked) {
        closeDoor();
        unlockInProgress = false;
        displayStatus("LOCKED");
        Blynk.virtualWrite(V1, "LOCKED");
      }
    }
  }
  else {
    isPhyButtonPressed = false;
  }
}

void checkPIR() {
  int currentHour = hour();
  int sensorValue = analogRead(PIR_PIN);
   if (currentHour >= 20 || currentHour < 6) { 
    if (sensorValue > 512) { 
      Blynk.logEvent("pir_triggered");
      Serial.println("PIR TRIGGERED!");
    }
   }

  delay(1500); 
}
#include "DFRobotDFPlayerMini.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_idf_version.h>
#include <esp_now.h>

// ===================== PINS =====================
static const int DF_RX_PIN = 16; // ESP32 RX pin connected to DFPlayer TX
static const int DF_TX_PIN = 17; // ESP32 TX pin connected to DFPlayer RX
static const int POT_PIN = 36;   // ADC pin connected to the potentiometer middle pin
static const int BATTERY_PIN = 35; // ADC pin connected to the battery voltage divider output

// ===================== LCD =====================
LiquidCrystal_I2C lcd(0x27, 16, 2); // Use 0x3F if the LCD module has a different I2C address

// ===================== DFPLAYER CONFIGURATION =====================
HardwareSerial FPSerial(1); // Hardware serial port used for DFPlayer communication
DFRobotDFPlayerMini myDFPlayer;

// ===================== SYSTEM SETTINGS =====================
static const uint8_t MIN_CMD = 1; // Minimum valid command number
static const uint8_t MAX_CMD = 10; // Maximum valid command number
static const unsigned long POT_INTERVAL_MS = 120; // Potentiometer reading interval

// Data packet structure received through ESP-NOW
typedef struct {
  uint8_t fileNumber;
} ReceiveData;

// This variable is updated inside the ESP-NOW receive callback
volatile uint8_t pendingCommand = 0;

// Runtime state variables
int currentVolume = -1;
uint8_t currentCommand = 0;
bool lcdNeedsUpdate = true;
unsigned long lastPotMillis = 0;

// --------------------------------------------------
const char *getCommandText(uint8_t cmd) {
  switch (cmd) {
  case 1:
    return "Stop";
  case 2:
    return "Listen";
  case 3:
    return "Go go";
  case 4:
    return "Come";
  case 5:
    return "Stick together";
  case 6:
    return "Slow down";
  case 7:
    return "Fall back";
  case 8:
    return "Take cover";
  case 9:
    return "Move right";
  case 10:
    return "Move left";
  default:
    return "Waiting...";
  }
}

// --------------------------------------------------
void updateLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(getCommandText(currentCommand));

  lcd.setCursor(0, 1);
  lcd.print("Vol: ");
  lcd.print(currentVolume);
  lcd.print("/30");

  int batPct = getBatteryPercentage();
  lcd.setCursor(11, 1);
  if (batPct < 100)
    lcd.print(" ");
  if (batPct < 10)
    lcd.print("  ");
  lcd.print(batPct);
  lcd.print("%");
}

// --------------------------------------------------
void applyVolumeFromPot() {
  int raw = analogRead(POT_PIN);            // 0..4095
  int newVolume = map(raw, 0, 4095, 0, 30); // 0..30
  newVolume = constrain(newVolume, 0, 30);

  if (newVolume != currentVolume) {
    currentVolume = newVolume;
    myDFPlayer.volume(currentVolume);

    Serial.print("­şöè Volume level: ");
    Serial.println(currentVolume);

    lcdNeedsUpdate = true;
  }
}

// --------------------------------------------------
int getBatteryPercentage() {
  int raw = analogRead(BATTERY_PIN);

  // Convert the raw ADC value to the voltage measured at the ADC pin
  float vOut = (raw / 4095.0) * 3.3;

  // Estimate the actual battery voltage using the 20k-10k voltage divider ratio
  // Vbat = Vout * ((20k + 10k) / 10k) = Vout * 3.0
  float vBat = vOut * 3.0;

  float maxVoltage = 9.0;  // Voltage considered as 100%
  float minVoltage = 6.0;  // Voltage considered as 0%

  if (vBat >= maxVoltage) {
    return 100;
  }

  if (vBat <= minVoltage) {
    return 0;
  }

  // Divide the battery voltage range into 20 steps
  // Each step represents a 5% battery level change
  int stepPercent = 5;
  int numberOfSteps = 20;

  float voltageStep = (maxVoltage - minVoltage) / numberOfSteps;

  int stepIndex = (int)((vBat - minVoltage) / voltageStep);

  int percentage = stepIndex * stepPercent;

  if (percentage > 100) percentage = 100;
  if (percentage < 0) percentage = 0;

  return percentage;
}

// --------------------------------------------------
void playCommand(uint8_t cmd) {
  currentCommand = cmd;

  Serial.print("­şô® Received command: ");
  Serial.println(cmd);

  Serial.print("­şÄÁ Playing: mp3/");

  // Print the corresponding MP3 file name for debugging
  if (cmd < 10)
    Serial.print("000");
  else if (cmd < 100)
    Serial.print("00");
  Serial.print(cmd);
  Serial.println(".mp3");

// Play the command audio file from the MP3 folder
  myDFPlayer.playMp3Folder(cmd);

  lcdNeedsUpdate = true;
}

// --------------------------------------------------
// ESP-NOW callback
#if ESP_IDF_VERSION_MAJOR >= 5
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData,
                int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  if (len != sizeof(ReceiveData)) {
    Serial.println("ÔÜá´©Å Wrong data size received");
    return;
  }

  ReceiveData temp;
  memcpy(&temp, incomingData, sizeof(temp));

  if (temp.fileNumber >= MIN_CMD && temp.fileNumber <= MAX_CMD) {
    pendingCommand = temp.fileNumber;
  } else {
    Serial.print("ÔÜá´©Å Invalid command: ");
    Serial.println(temp.fileNumber);
  }
}

// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== RECEIVER: ESP-NOW + DFPlayer + LCD + Pot ===");

  // Pot & Battery
  pinMode(POT_PIN, INPUT);
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12);

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System");
  lcd.setCursor(0, 1);
  lcd.print("Starting");
  delay(1000);

  // DFPlayer
  FPSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
  delay(300);

  Serial.println("DFPlayer starting...");

  if (!myDFPlayer.begin(FPSerial, true, true)) {
    Serial.println("ÔØî DFPlayer could not be started!");
    Serial.println("1) Check TX/RX connection");
    Serial.println("2) Check SD card");
    Serial.println("3) Check 5V and GND connection");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DFPlayer Error");
    lcd.setCursor(0, 1);
    lcd.print("Check it");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Ô£à DFPlayer ready");

  myDFPlayer.setTimeOut(500);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  delay(300);

  // Initial volume level
  applyVolumeFromPot();

  // WiFi Station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);

  Serial.print("­şôı Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Start ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ÔØî ESP-NOW initialization error!");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESP-NOW Error");
    while (true) {
      delay(1000);
    }
  }

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Ô£à ESP-NOW ready");
  Serial.println("­şÄğ Waiting for command...");

  currentCommand = 0;
  lcdNeedsUpdate = true;
  updateLCD();
}

// --------------------------------------------------
void loop() {
  // 1) Read the potentiometer periodically and update the DFPlayer volume
  if (millis() - lastPotMillis >= POT_INTERVAL_MS) {
    lastPotMillis = millis();
    applyVolumeFromPot();
  }

  // 2) Process a newly received ESP-NOW command, if available
  if (pendingCommand != 0) {
    noInterrupts();
    uint8_t cmd = pendingCommand;
    pendingCommand = 0;
    interrupts();

    playCommand(cmd);
  }

  // 3) Refresh the LCD periodically to update the battery level
  static unsigned long lastBatteryUpdate = 0;
  if (millis() - lastBatteryUpdate >= 10000) {
    lastBatteryUpdate = millis();
    lcdNeedsUpdate = true;
  }

  if (lcdNeedsUpdate) {
    lcdNeedsUpdate = false;
    updateLCD();
  }

  // 4) Check DFPlayer status messages and report playback or error events
  if (myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();
    int value = myDFPlayer.read();

    if (type == DFPlayerPlayFinished) {
      Serial.print("Ô£à File playback completed: ");
      Serial.println(value);
    } else if (type == DFPlayerError) {
      Serial.print("ÔÜá´©Å DFPlayer error code: ");
      Serial.println(value);
    }
  }

  delay(10);
}

#include "DFRobotDFPlayerMini.h"
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_idf_version.h>
#include <esp_now.h>

// ===================== PINS =====================
static const int DF_RX_PIN = 16; // ESP32 RX  <- DFPlayer TX
static const int DF_TX_PIN = 17; // ESP32 TX  -> DFPlayer RX
static const int POT_PIN = 36;   // Pot middle pin (ADC1)
static const int BATTERY_PIN = 34; // 'sp' pin (SVN) or change to correct ADC pin

// ===================== LCD =====================
LiquidCrystal_I2C lcd(0x27, 16, 2); // Change to 0x3F if necessary

// ===================== DFPLAYER =====================
HardwareSerial FPSerial(1);
DFRobotDFPlayerMini myDFPlayer;

// ===================== SETTINGS =====================
static const uint8_t MIN_CMD = 1;
static const uint8_t MAX_CMD = 10;
static const unsigned long POT_INTERVAL_MS = 120;

// ESP-NOW data structure
typedef struct {
  uint8_t fileNumber;
} ReceiveData;

// We will only set this in the callback
volatile uint8_t pendingCommand = 0;

// State variables
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
    lcd.print(" ");
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

    Serial.print("🔊 Volume level: ");
    Serial.println(currentVolume);

    lcdNeedsUpdate = true;
  }
}

// --------------------------------------------------
int getBatteryPercentage() {
  int raw = analogRead(BATTERY_PIN);
  float vOut = (raw / 4095.0) * 3.3; // ESP32 ADC reference is ~3.3V

  // Voltage divider: R1 = 20k, R2 = 10k
  // Vbat = Vout * ((R1 + R2) / R2) = Vout * ((20k + 10k) / 10k) = Vout * 3.0
  float vBat = vOut * 3.0;

  // Assuming a 9V battery (9.5V max, 6.0V min)
  float maxVoltage = 9.5;
  float minVoltage = 6.0;

  if (vBat >= maxVoltage)
    return 100;
  if (vBat <= minVoltage)
    return 0;

  return (int)(((vBat - minVoltage) / (maxVoltage - minVoltage)) * 100.0);
}

// --------------------------------------------------
void playCommand(uint8_t cmd) {
  currentCommand = cmd;

  Serial.print("📩 Received command: ");
  Serial.println(cmd);

  Serial.print("🎵 Playing: mp3/");
  // Zeros can be added to see the played file format more clearly in the
  // terminal
  if (cmd < 10)
    Serial.print("000");
  else if (cmd < 100)
    Serial.print("00");
  Serial.print(cmd);
  Serial.println(".mp3");

  // INSTEAD OF myDFPlayer.play(cmd) WE USE THE FOLLOWING:
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
    Serial.println("⚠️ Wrong data size received");
    return;
  }

  ReceiveData temp;
  memcpy(&temp, incomingData, sizeof(temp));

  if (temp.fileNumber >= MIN_CMD && temp.fileNumber <= MAX_CMD) {
    pendingCommand = temp.fileNumber;
  } else {
    Serial.print("⚠️ Invalid command: ");
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
    Serial.println("❌ DFPlayer could not be started!");
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

  Serial.println("✅ DFPlayer ready");

  myDFPlayer.setTimeOut(500);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  delay(300);

  // Initial volume level
  applyVolumeFromPot();

  // WiFi Station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);

  Serial.print("📍 Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Start ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW initialization error!");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESP-NOW Error");
    while (true) {
      delay(1000);
    }
  }

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("✅ ESP-NOW ready");
  Serial.println("🎧 Waiting for command...");

  currentCommand = 0;
  lcdNeedsUpdate = true;
  updateLCD();
}

// --------------------------------------------------
void loop() {
  // 1) Volume adjustment with Pot
  if (millis() - lastPotMillis >= POT_INTERVAL_MS) {
    lastPotMillis = millis();
    applyVolumeFromPot();
  }

  // 2) Play the received command here
  if (pendingCommand != 0) {
    noInterrupts();
    uint8_t cmd = pendingCommand;
    pendingCommand = 0;
    interrupts();

    playCommand(cmd);
  }

  // 3) Update LCD periodically or when needed
  static unsigned long lastBatteryUpdate = 0;
  if (millis() - lastBatteryUpdate >= 10000) {
    lastBatteryUpdate = millis();
    lcdNeedsUpdate = true;
  }

  if (lcdNeedsUpdate) {
    lcdNeedsUpdate = false;
    updateLCD();
  }

  // 4) DFPlayer status tracking
  if (myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();
    int value = myDFPlayer.read();

    if (type == DFPlayerPlayFinished) {
      Serial.print("✅ File playback completed: ");
      Serial.println(value);
    } else if (type == DFPlayerError) {
      Serial.print("⚠️ DFPlayer error code: ");
      Serial.println(value);
    }
  }

  delay(10);
}
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp_idf_version.h>
#include "DFRobotDFPlayerMini.h"

// ===================== PINLER =====================
static const int DF_RX_PIN = 16;   // ESP32 RX  <- DFPlayer TX
static const int DF_TX_PIN = 17;   // ESP32 TX  -> DFPlayer RX
static const int POT_PIN   = 36;   // Pot orta uç (ADC1)

// ===================== LCD =====================
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Gerekirse 0x3F yap

// ===================== DFPLAYER =====================
HardwareSerial FPSerial(1);
DFRobotDFPlayerMini myDFPlayer;

// ===================== AYARLAR =====================
static const uint8_t MIN_CMD = 1;
static const uint8_t MAX_CMD = 10;
static const unsigned long POT_INTERVAL_MS = 120;

// ESP-NOW veri yapısı
typedef struct {
  uint8_t fileNumber;
} ReceiveData;

// Callback içinde sadece bunu set edeceğiz
volatile uint8_t pendingCommand = 0;

// Durum değişkenleri
int currentVolume = -1;
uint8_t currentCommand = 0;
bool lcdNeedsUpdate = true;
unsigned long lastPotMillis = 0;

// --------------------------------------------------
const char* getCommandText(uint8_t cmd) {
  switch (cmd) {
    case 1: return "Stop";
    case 2: return "Listen";
    case 3: return "Come";
    case 4: return "Stick together";
    case 5: return "Go go";
    case 6: return "Slow down";
    case 7: return "Fall back";
    case 8: return "Take cover";
    case 9: return "Move right";
    case 10: return "Move left";
    default: return "Bekleniyor...";
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
}

// --------------------------------------------------
void applyVolumeFromPot() {
  int raw = analogRead(POT_PIN);               // 0..4095
  int newVolume = map(raw, 0, 4095, 0, 30);   // 0..30
  newVolume = constrain(newVolume, 0, 30);

  if (newVolume != currentVolume) {
    currentVolume = newVolume;
    myDFPlayer.volume(currentVolume);

    Serial.print("🔊 Ses seviyesi: ");
    Serial.println(currentVolume);

    lcdNeedsUpdate = true;
  }
}

// --------------------------------------------------
// --------------------------------------------------
void playCommand(uint8_t cmd) {
  currentCommand = cmd;

  Serial.print("📩 Gelen komut: ");
  Serial.println(cmd);

  Serial.print("🎵 Caliniyor: mp3/");
  // Çalınan dosyanın formatını terminalde daha net görmek için sıfır eklenebilir
  if(cmd < 10) Serial.print("000");
  else if(cmd < 100) Serial.print("00");
  Serial.print(cmd);
  Serial.println(".mp3");

  // myDFPlayer.play(cmd) YERİNE AŞAĞIDAKİNİ KULLANIYORUZ:
  myDFPlayer.playMp3Folder(cmd); 
  
  lcdNeedsUpdate = true;
}

// --------------------------------------------------
// ESP-NOW callback
#if ESP_IDF_VERSION_MAJOR >= 5
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
  if (len != sizeof(ReceiveData)) {
    Serial.println("⚠️ Yanlis veri boyutu geldi");
    return;
  }

  ReceiveData temp;
  memcpy(&temp, incomingData, sizeof(temp));

  if (temp.fileNumber >= MIN_CMD && temp.fileNumber <= MAX_CMD) {
    pendingCommand = temp.fileNumber;
  } else {
    Serial.print("⚠️ Gecersiz komut: ");
    Serial.println(temp.fileNumber);
  }
}

// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== RECEIVER: ESP-NOW + DFPlayer + LCD + Pot ===");

  // Pot
  pinMode(POT_PIN, INPUT);
  analogReadResolution(12);

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sistem");
  lcd.setCursor(0, 1);
  lcd.print("Baslatiliyor");
  delay(1000);

  // DFPlayer
  FPSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
  delay(300);

  Serial.println("DFPlayer baslatiliyor...");

  if (!myDFPlayer.begin(FPSerial, true, true)) {
    Serial.println("❌ DFPlayer baslatilamadi!");
    Serial.println("1) TX/RX baglantisini kontrol et");
    Serial.println("2) SD karti kontrol et");
    Serial.println("3) 5V ve GND baglantisini kontrol et");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DFPlayer Hata");
    lcd.setCursor(0, 1);
    lcd.print("Kontrol et");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("✅ DFPlayer hazir");

  myDFPlayer.setTimeOut(500);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  delay(300);

  // Ilk ses seviyesi
  applyVolumeFromPot();

  // WiFi Station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);

  Serial.print("📍 Receiver MAC Adresi: ");
  Serial.println(WiFi.macAddress());

  // ESP-NOW baslat
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW baslatma hatasi!");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESP-NOW Hata");
    while (true) {
      delay(1000);
    }
  }

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("✅ ESP-NOW hazir");
  Serial.println("🎧 Komut bekleniyor...");

  currentCommand = 0;
  lcdNeedsUpdate = true;
  updateLCD();
}

// --------------------------------------------------
void loop() {
  // 1) Pot ile ses ayari
  if (millis() - lastPotMillis >= POT_INTERVAL_MS) {
    lastPotMillis = millis();
    applyVolumeFromPot();
  }

  // 2) Gelen komutu burada cal
  if (pendingCommand != 0) {
    noInterrupts();
    uint8_t cmd = pendingCommand;
    pendingCommand = 0;
    interrupts();

    playCommand(cmd);
  }

  // 3) LCD'yi sadece gerekince guncelle
  if (lcdNeedsUpdate) {
    lcdNeedsUpdate = false;
    updateLCD();
  }

  // 4) DFPlayer durum takibi
  if (myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();
    int value = myDFPlayer.read();

    if (type == DFPlayerPlayFinished) {
      Serial.print("✅ Dosya calma tamamlandi: ");
      Serial.println(value);
    }
    else if (type == DFPlayerError) {
      Serial.print("⚠️ DFPlayer hata kodu: ");
      Serial.println(value);
    }
  }

  delay(10);
}
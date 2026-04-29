#include <Wire.h>
#include <Kalman.h>
#include <WiFi.h>
#include <esp_now.h>
#include "model.h" // Eğittiğimiz SVM modelini dahil ediyoruz
#include <esp_idf_version.h>

// ==================== PIN DEFINITIONS ====================
#define FLEX_1 25
#define FLEX_2 33
#define FLEX_3 32
#define FLEX_4 35
#define FLEX_5 34

#define BUTTON_PIN 13
#define LED_R_PIN  15
#define LED_G_PIN  2
#define LED_B_PIN  0

#define SDA_PIN 21
#define SCL_PIN 22

// ==================== MPU6050 ====================
#define IMU_ADDR 0x68

const int flexPins[5] = {FLEX_1, FLEX_2, FLEX_3, FLEX_4, FLEX_5};

Kalman kalmanX;
Kalman kalmanY;

uint32_t timer = 0;
uint8_t  i2cData[14];
const uint16_t I2C_TIMEOUT = 1000;

float cal_open[5]  = {0};
float cal_close[5] = {0};

float acc_offset[3]  = {0};
float gyro_offset[3] = {0};

// ==================== ESP-NOW ====================
// Receiver kartinin MAC adresini buraya yaz
uint8_t receiverMAC[] = {0x08, 0xA6, 0xF7, 0x47, 0x5C, 0x3C};

typedef struct {
  uint8_t fileNumber;
} SendData;

SendData sendData;

// ==================== DURUM (STATE) TANIMLARI ====================
enum State {
  IDLE,
  CALIBRATING,
  READY,
  INFERENCING
};

State currentState = IDLE;

// ==================== BUTON DEBOUNCE ====================
unsigned long lastButtonPress = 0;
bool lastButtonState = HIGH;

// ==================== SVM FEATURE BUFFER ====================
// SVM yerine DecisionTree kullanıyoruz (model.h'de tanımlı)
Eloquent::ML::Port::DecisionTree clf;

// Toplanan ham veriler: [13 sensör][50 örnek]
float raw_data[13][50];

// SVM'in beklediği standardize edilmiş 650 boyutlu düzleştirilmiş vektör (13 x 50)
float svm_features[650];

// ==================== I2C HELPERS ====================
uint8_t i2cWrite(uint8_t registerAddress, uint8_t data, bool sendStop) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(registerAddress);
  Wire.write(data);
  return Wire.endTransmission(sendStop);
}

uint8_t i2cWrite(uint8_t registerAddress, uint8_t *data, uint8_t length, bool sendStop) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(registerAddress);
  Wire.write(data, length);
  return Wire.endTransmission(sendStop);
}

uint8_t i2cRead(uint8_t registerAddress, uint8_t *data, uint8_t nbytes) {
  uint32_t timeOutTimer;
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(registerAddress);
  uint8_t rcode = Wire.endTransmission(false);
  if (rcode) return rcode;

  Wire.requestFrom((uint8_t)IMU_ADDR, nbytes, (uint8_t)true);
  for (uint8_t i = 0; i < nbytes; i++) {
    if (Wire.available()) {
      data[i] = Wire.read();
    } else {
      timeOutTimer = micros();
      while (((micros() - timeOutTimer) < I2C_TIMEOUT) && !Wire.available());
      if (Wire.available()) data[i] = Wire.read();
      else return 5;
    }
  }
  return 0;
}

// ==================== LED ====================
void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R_PIN, r);
  digitalWrite(LED_G_PIN, g);
  digitalWrite(LED_B_PIN, b);
}

// ==================== ESP-NOW ====================
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("ESP-NOW gonderim: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "BASARILI" : "BASARISIZ");
}

void initEspNowSender() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.print("Sender MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("HATA: ESP-NOW baslatilamadi!");
    while (true) delay(1000);
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK && result != ESP_ERR_ESPNOW_EXIST) {
    Serial.print("HATA: Peer eklenemedi, kod = ");
    Serial.println(result);
    while (true) delay(1000);
  }

  Serial.println("ESP-NOW sender hazir.");
}

// Model.h'deki etiket isimlerini komut numaralarına eşliyoruz.
// README'deki 10 hareket karşılıkları:
uint8_t labelToCommand(const char* label) {
  if (strcmp(label, "stop") == 0)              return 1;  // Dur
  if (strcmp(label, "hear") == 0)              return 2;  // Dinle
  if (strcmp(label, "five") == 0)              return 3;  // İleri (Go go)
  if (strcmp(label, "four") == 0)              return 4;  // Gel (Come)
  if (strcmp(label, "one") == 0)               return 5;  // Birlikte kal
  if (strcmp(label, "two") == 0)               return 6;  // Yavaşla
  if (strcmp(label, "three") == 0)             return 7;  // Geri çekil
  if (strcmp(label, "enemy") == 0)             return 8;  // Siper al
  if (strcmp(label, "watch") == 0)             return 9;  // Sağa git
  if (strcmp(label, "you") == 0)               return 10; // Sola git
  
  // Diğer etiketler (28 sınıftan kalan 18'i) komut olarak gönderilmez
  return 0;
}

void sendCommandToReceiver(uint8_t cmd) {
  if (cmd < 1 || cmd > 10) {
    Serial.println("Gecersiz komut, gonderilmiyor.");
    return;
  }

  sendData.fileNumber = cmd;

  esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&sendData, sizeof(sendData));

  Serial.print("Receiver'a gonderilen komut: ");
  Serial.print(cmd);
  Serial.print(" -> ");

  if (result == ESP_OK) {
    Serial.println("paket gonderildi");
  } else {
    Serial.print("gonderim hatasi: ");
    Serial.println(result);
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(LED_R_PIN,  OUTPUT);
  pinMode(LED_G_PIN,  OUTPUT);
  pinMode(LED_B_PIN,  OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000UL);

  initEspNowSender();

  i2cData[0] = 7;     // Sample rate divider → 1000 Hz
  i2cData[1] = 0x00;  // DLPF off
  i2cData[2] = 0x00;  // Gyro  ±250 °/s
  i2cData[3] = 0x00;  // Accel ±2 g
  while (i2cWrite(0x19, i2cData, 4, false));
  while (i2cWrite(0x6B, 0x01, true));
  delay(100);

  Serial.println("MSG: Sistem baslatildi. Kalibrasyon icin butona basin.");
  setLED(0, 0, 1); // Mavi → IDLE
}

// ==================== BUTON DEBOUNCE ====================
bool isButtonPressed() {
  bool reading = digitalRead(BUTTON_PIN);
  bool pressed = false;
  if (reading != lastButtonState) {
    if (reading == LOW && (millis() - lastButtonPress) > 200) {
      pressed = true;
      lastButtonPress = millis();
    }
  }
  lastButtonState = reading;
  return pressed;
}

// ==================== KALİBRASYON ====================
void runCalibration() {
  Serial.println("MSG: Elinizi TUMUYLE ACIK (DUZ) tutun! (2.5 sn)");
  setLED(1, 1, 0); // Sari
  delay(2500);

  long f_sum[5] = {0};
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 5; j++) f_sum[j] += analogRead(flexPins[j]);
    delay(20);
  }
  for (int j = 0; j < 5; j++) cal_open[j] = f_sum[j] / 20.0f;

  Serial.println("MSG: Elinizi TUMUYLE KAPATIN (YUMRUK)! (2.5 sn)");
  setLED(1, 0, 0); // Kirmizi
  delay(2500);

  for (int j = 0; j < 5; j++) f_sum[j] = 0;
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 5; j++) f_sum[j] += analogRead(flexPins[j]);
    delay(20);
  }
  for (int j = 0; j < 5; j++) cal_close[j] = f_sum[j] / 20.0f;

  Serial.println("MSG: Elinizi SABIT TUTUN (Gyro/Accel kalibre ediliyor)...");
  setLED(1, 0, 1); // Mor
  delay(2500);

  float a_sum[3] = {0};
  float g_sum[3] = {0};
  const int mpu_count = 50;

  for (int i = 0; i < mpu_count; i++) {
    while (i2cRead(0x3B, i2cData, 14));
    a_sum[0] += (int16_t)((i2cData[0] << 8) | i2cData[1]);
    a_sum[1] += (int16_t)((i2cData[2] << 8) | i2cData[3]);
    a_sum[2] += (int16_t)((i2cData[4] << 8) | i2cData[5]) - 16384.0f;
    g_sum[0] += (int16_t)((i2cData[8] << 8) | i2cData[9]);
    g_sum[1] += (int16_t)((i2cData[10] << 8) | i2cData[11]);
    g_sum[2] += (int16_t)((i2cData[12] << 8) | i2cData[13]);
    delay(10);
  }
  for (int j = 0; j < 3; j++) {
    acc_offset[j]  = a_sum[j] / mpu_count;
    gyro_offset[j] = g_sum[j] / mpu_count;
  }

  while (i2cRead(0x3B, i2cData, 14));
  double aX = (int16_t)((i2cData[0] << 8) | i2cData[1]);
  double aY = (int16_t)((i2cData[2] << 8) | i2cData[3]);
  double aZ = (int16_t)((i2cData[4] << 8) | i2cData[5]);
  kalmanX.setAngle(atan2(aY, aZ) * RAD_TO_DEG);
  kalmanY.setAngle(atan(-aX / sqrt(aY * aY + aZ * aZ)) * RAD_TO_DEG);
  timer = micros();

  Serial.println("MSG: KALIBRASYON TAMAMLANDI! Tahmin icin butona basin.");
}

// ==================== INFERENCE ====================
void performInference() {
  Serial.println("MSG: HAREKETI YAPIN! (Veri toplaniyor...)");
  setLED(0, 1, 1); // Cyan

  const int num_sensors        = 13; // 5 Flex + 2 Kalman + 6 IMU
  const int num_readings       = 50; // Eğitildiği gibi sabit 50 okuma alıyoruz
  const uint32_t sample_interval_ms = 60; // 50 * 60 = 3000ms (3 Saniyelik süreç)

  Serial.print("MSG: Toplanacak ornek sayisi: "); Serial.println(num_readings);
  Serial.print("MSG: Ornek araligi (ms): ");      Serial.println(sample_interval_ms);

  for (int ix = 0; ix < num_readings; ix++) {
    unsigned long loopStart = millis();

    // 1. Flex sensör okuma & normalizasyon
    float flex_norm[5];
    for (int i = 0; i < 5; i++) {
      float raw  = analogRead(flexPins[i]);
      float diff = cal_close[i] - cal_open[i];
      if (abs(diff) < 1.0f) {
        flex_norm[i] = 0.0f;
      } else {
        flex_norm[i] = constrain((raw - cal_open[i]) / diff, 0.0f, 1.0f);
      }
    }

    // 2. MPU-6050 okuma
    while (i2cRead(0x3B, i2cData, 14));
    double accX  = (int16_t)((i2cData[0]  << 8) | i2cData[1]);
    double accY  = (int16_t)((i2cData[2]  << 8) | i2cData[3]);
    double accZ  = (int16_t)((i2cData[4]  << 8) | i2cData[5]);
    double gyroX = (int16_t)((i2cData[8]  << 8) | i2cData[9]);
    double gyroY = (int16_t)((i2cData[10] << 8) | i2cData[11]);
    double gyroZ = (int16_t)((i2cData[12] << 8) | i2cData[13]);

    double dt = (double)(micros() - timer) / 1000000.0;
    timer = micros();

    double r = atan2(accY, accZ) * RAD_TO_DEG;
    double p = atan(-accX / sqrt(accY * accY + accZ * accZ)) * RAD_TO_DEG;

    float kalRoll  = kalmanX.getAngle(r, gyroX / 131.0, dt);
    float kalPitch = kalmanY.getAngle(p, gyroY / 131.0, dt);

    float f_ax = accX  - acc_offset[0];
    float f_ay = accY  - acc_offset[1];
    float f_az = accZ  - acc_offset[2];
    float f_gx = gyroX - gyro_offset[0];
    float f_gy = gyroY - gyro_offset[1];
    float f_gz = gyroZ - gyro_offset[2];

    // Okunan değerleri sırayla 13 sensörün satırına kaydediyoruz
    raw_data[0][ix] = flex_norm[0];
    raw_data[1][ix] = flex_norm[1];
    raw_data[2][ix] = flex_norm[2];
    raw_data[3][ix] = flex_norm[3];
    raw_data[4][ix] = flex_norm[4];
    raw_data[5][ix] = kalPitch;
    raw_data[6][ix] = kalRoll;
    raw_data[7][ix] = f_ax;
    raw_data[8][ix] = f_ay;
    raw_data[9][ix] = f_az;
    raw_data[10][ix] = f_gx;
    raw_data[11][ix] = f_gy;
    raw_data[12][ix] = f_gz;

    unsigned long elapsed = millis() - loopStart;
    if (elapsed < sample_interval_ms) {
      delay(sample_interval_ms - elapsed);
    }
  }

  // ==================== Z-SCORE NORMALİZASYONU VE FLATTEN ====================
  // Eğittiğimiz Python kodundaki gibi değerleri burada standartlaştırıyoruz
  for (int s = 0; s < num_sensors; s++) {
    float sum = 0;
    for (int i = 0; i < num_readings; i++) sum += raw_data[s][i];
    float mean = sum / num_readings;

    float sq_sum = 0;
    for (int i = 0; i < num_readings; i++) sq_sum += (raw_data[s][i] - mean) * (raw_data[s][i] - mean);
    float stddev = sqrt(sq_sum / num_readings);
    
    if (stddev < 0.0001f) stddev = 1.0f; // Sıfıra bölme hatasını önlemek için

    // Vektörü tek boyutlu(650 array) haline getirerek doğrudan yazdırıyoruz
    for (int i = 0; i < num_readings; i++) {
      svm_features[s * num_readings + i] = (raw_data[s][i] - mean) / stddev;
    }
  }

  // ==================== INFERENCE (SVM) ====================
  Serial.println("MSG: SVM Modeli calistiriliyor...");
  
  // "model.h" ile doğrudan sonucu string ("one", "enemy" vb.) olarak alıyoruz
  const char* best_label = clf.predictLabel(svm_features);

  // ==================== SONUÇLARI YAZDIR ====================
  Serial.println("\n=========================================");
  Serial.print(">>> KARAR: ");
  Serial.print(best_label);
  Serial.println(" <<<");
  Serial.println("=========================================\n");

  setLED(0, 1, 0); // Yeşil → başarılı tanıma

  uint8_t cmd = labelToCommand(best_label);

  if (cmd != 0) {
    sendCommandToReceiver(cmd);
  } else {
    Serial.println("UYARI: Label komuta cevrilemedi, gonderim yapilmadi.");
  }
}

// ==================== LOOP ====================
void loop() {
  bool btnPressed = isButtonPressed();

  switch (currentState) {
    case IDLE:
      if (btnPressed) {
        currentState = CALIBRATING;
        runCalibration();
        currentState = READY;
        setLED(0, 1, 0); // Yesil → READY
      }
      break;

    case READY:
      if (btnPressed) {
        currentState = INFERENCING;
        performInference();
        currentState = READY;
      }
      break;

    case CALIBRATING:
    case INFERENCING:
      break;
  }
}
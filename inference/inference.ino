#include <Wire.h>
#include <Kalman.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ILTER-AKKE_1.3.3_inferencing.h>
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
#define BATTERY_PIN 39 // SVP pini (SP)

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
uint8_t receiverMAC[] = {0x08, 0xA6, 0xF7, 0x47, 0x5C, 0x3C};

typedef struct {
  uint8_t fileNumber;
} SendData;

SendData sendData;

// ==================== STATE ====================
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

// ==================== BATTERY ====================
unsigned long lastBatteryMillis = 0;
const unsigned long BATTERY_INTERVAL_MS = 2000;
bool isBatteryLowState = false;

void checkBattery() {
  int raw = analogRead(BATTERY_PIN);
  float vOut = (raw / 4095.0) * 3.3; // ESP32 ADC reference ~3.3V
  
  // Voltage divider: R1 = 20k, R2 = 10k
  float vBat = vOut * 3.0;

  float maxVoltage = 9.5;
  float minVoltage = 6.0;

  int percent = 0;
  if (vBat >= maxVoltage) {
    percent = 100;
  } else if (vBat <= minVoltage) {
    percent = 0;
  } else {
    percent = (int)(((vBat - minVoltage) / (maxVoltage - minVoltage)) * 100.0);
  }

  // To see the problem, let's print the voltage values to the Serial Monitor
  Serial.print("Battery Test -> RAW: "); Serial.print(raw);
  Serial.print(" | vOut (Pin): "); Serial.print(vOut);
  Serial.print("V | vBat (Real): "); Serial.print(vBat);
  Serial.print("V | Percentage: %"); Serial.println(percent);

  // If the battery voltage is below the limit, switch to low battery mode
  if (vBat < minVoltage) {
    isBatteryLowState = true;
  } else {
    isBatteryLowState = false;
  }
}

// ==================== Edge Impulse FEATURE BUFFER ====================
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

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
  Serial.print("ESP-NOW send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAILED");
}

void initEspNowSender() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.print("Sender MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: ESP-NOW initialization failed!");
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
    Serial.print("ERROR: Peer could not be added, code = ");
    Serial.println(result);
    while (true) delay(1000);
  }

  Serial.println("ESP-NOW sender is ready.");
}

// Labeled data comparison
uint8_t labelToCommand(const char* label) {
  if (strcmp(label, "Hareket_1") == 0) return 1;
  if (strcmp(label, "Hareket_2") == 0) return 2;
  if (strcmp(label, "Hareket_3") == 0) return 3;
  if (strcmp(label, "Hareket_4") == 0) return 4;
  if (strcmp(label, "Hareket_5") == 0) return 5;
  if (strcmp(label, "Hareket_6") == 0) return 6;
  if (strcmp(label, "Hareket_7") == 0) return 7;
  if (strcmp(label, "Hareket_8") == 0) return 8;
  if (strcmp(label, "Hareket_9") == 0) return 9;
  if (strcmp(label, "Hareket_10") == 0) return 10;

  return 0;
}

void sendCommandToReceiver(uint8_t cmd) {
  if (cmd < 1 || cmd > 10) {
    Serial.println("Invalid command, not sent.");
    return;
  }

  sendData.fileNumber = cmd;

  esp_err_t result = esp_now_send(receiverMAC, (uint8_t*)&sendData, sizeof(sendData));

  Serial.print("Command sent to receiver: ");
  Serial.print(cmd);
  Serial.print(" -> ");

  if (result == ESP_OK) {
    Serial.println("packet sent");
  } else {
    Serial.print("send error: ");
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

  Serial.println("========== EI CLASSIFIER CONSTANTS ==========");
  Serial.print("EI_CLASSIFIER_FREQUENCY            : "); Serial.println(EI_CLASSIFIER_FREQUENCY);
  Serial.print("EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE : "); Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  Serial.print("EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME: "); Serial.println(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME);
  Serial.print("EI_CLASSIFIER_RAW_SAMPLE_COUNT     : "); Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
  Serial.print("EI_CLASSIFIER_LABEL_COUNT          : "); Serial.println(EI_CLASSIFIER_LABEL_COUNT);
  Serial.println("=================================================");

  Serial.println("MSG: System started. Press button for calibration.");
  
  // Booting: Blinking White
  for (int i = 0; i < 3; i++) {
    setLED(1, 1, 1);
    delay(200);
    setLED(0, 0, 0);
    delay(200);
  }
  
  setLED(0, 0, 1); // Blue → IDLE
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

// ==================== Calibration ====================
void runCalibration() {
  Serial.println("MSG: Keep your hand FULLY OPEN (STRAIGHT)! (2.5 sn)");
  setLED(1, 1, 0); // Yellow
  delay(2500);

  long f_sum[5] = {0};
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 5; j++) f_sum[j] += analogRead(flexPins[j]);
    delay(20);
  }
  for (int j = 0; j < 5; j++) cal_open[j] = f_sum[j] / 20.0f;

  Serial.println("MSG: Keep your hand FULLY CLOSED (FIST)! (2.5 sn)");
  setLED(1, 0, 0); // Red
  delay(2500);

  for (int j = 0; j < 5; j++) f_sum[j] = 0;
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 5; j++) f_sum[j] += analogRead(flexPins[j]);
    delay(20);
  }
  for (int j = 0; j < 5; j++) cal_close[j] = f_sum[j] / 20.0f;

  Serial.println("MSG: Keep your hand STEADY (Gyro/Accel calibrating)...");
  setLED(1, 0, 1); // Purple
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

  Serial.println("MSG: CALIBRATION COMPLETED! Press the button to start.");
}

// ==================== INFERENCE ====================
void performInference() {
  Serial.println("MSG: MAKE THE MOVE! (Data collection...)");
  setLED(1, 1, 0); // Processing -> Solid Yellow (from image)

  const int num_sensors        = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
  const int num_readings       = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  const uint32_t sample_interval_ms = (uint32_t)(1000.0f / EI_CLASSIFIER_FREQUENCY);

  Serial.print("MSG: Number of samples to collect: "); Serial.println(num_readings);
  Serial.print("MSG: Sample interval (ms): ");      Serial.println(sample_interval_ms);

  for (int ix = 0; ix < num_readings; ix++) {
    unsigned long loopStart = millis();

    // 1. Flex sensor reading & normalization
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

    // 2. MPU-6050 reading
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

    features[ix * num_sensors + 0]  = flex_norm[0];
    features[ix * num_sensors + 1]  = flex_norm[1];
    features[ix * num_sensors + 2]  = flex_norm[2];
    features[ix * num_sensors + 3]  = flex_norm[3];
    features[ix * num_sensors + 4]  = flex_norm[4];
    features[ix * num_sensors + 5]  = kalPitch;
    features[ix * num_sensors + 6]  = kalRoll;
    features[ix * num_sensors + 7]  = f_ax;
    features[ix * num_sensors + 8]  = f_ay;
    features[ix * num_sensors + 9]  = f_az;
    features[ix * num_sensors + 10] = f_gx;
    features[ix * num_sensors + 11] = f_gy;
    features[ix * num_sensors + 12] = f_gz;

    unsigned long elapsed = millis() - loopStart;
    if (elapsed < sample_interval_ms) {
      delay(sample_interval_ms - elapsed);
    }
  }

  // ==================== INFERENCE ====================
  Serial.println("MSG: Model inferencing...");

  signal_t signal;
  int err = numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
  if (err != 0) {
    ei_printf("ERROR: Signal generation failed (code: %d)\n", err);
    for(int i=0; i<2; i++){ setLED(1, 0, 0); delay(200); setLED(0, 0, 0); if(i<1) delay(200); }
    return;
  }

  ei_impulse_result_t result = {0};
  err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) {
    ei_printf("ERROR: Model inference failed (code: %d)\n", err);
    for(int i=0; i<2; i++){ setLED(1, 0, 0); delay(200); setLED(0, 0, 0); if(i<1) delay(200); }
    return;
  }

  // ==================== PRINT RESULTS ====================
  Serial.println("\n=========================================");
  Serial.println("INFERENCE RESULTS:");

  float max_score  = 0.0f;
  const char* best_label = "?";

  for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print("  ");
    Serial.print(result.classification[i].label);
    Serial.print(": %");
    Serial.println(result.classification[i].value * 100.0f, 2);

    if (result.classification[i].value > max_score) {
      max_score  = result.classification[i].value;
      best_label = result.classification[i].label;
    }
  }

  Serial.println("-----------------------------------------");
  Serial.print(">>> DECISION: ");
  Serial.print(best_label);
  Serial.print("  (Confidence: %");
  Serial.print(max_score * 100.0f, 2);
  Serial.println(") <<<");
  Serial.println("=========================================\n");

  // Güven eşiği kontrolü (%75)
  if (max_score >= 0.75f) {
    uint8_t cmd = labelToCommand(best_label);

    if (cmd != 0) {
      sendCommandToReceiver(cmd);
      // Success -> Blink Green (Once)
      setLED(0, 1, 0);
      delay(500);
      setLED(0, 0, 0);
    } else {
      Serial.println("WARNING: Label could not be converted to command, not sent.");
      // Error -> Blink Red (Twice)
      for(int i=0; i<2; i++){ setLED(1, 0, 0); delay(200); setLED(0, 0, 0); if(i<1) delay(200); }
    }

  } else {
    Serial.println("WARNING: Confidence threshold below (75%), result invalid!");
    // Error / Low Confidence -> Blink Red (Twice)
    for(int i=0; i<2; i++){
      setLED(1, 0, 0);
      delay(200);
      setLED(0, 0, 0);
      if(i<1) delay(200);
    }
  }
}

// ==================== LOOP ====================
void loop() {
  // Receiver.ino logic for periodic battery check
  if (millis() - lastBatteryMillis >= BATTERY_INTERVAL_MS) {
    lastBatteryMillis = millis();
    //checkBattery();
  }

  // Low Battery -> Blinking Red (Continuous)
  if (isBatteryLowState) {
    if ((millis() / 500) % 2 == 0) {
      setLED(1, 0, 0);
    } else {
      setLED(0, 0, 0);
    }
  } else {
    // Idle / Ready -> Solid Blue
    if (currentState == IDLE || currentState == READY) {
      setLED(0, 0, 1);
    }
  }

  bool btnPressed = isButtonPressed();

  switch (currentState) {
    case IDLE:
      if (btnPressed && !isBatteryLowState) {
        currentState = CALIBRATING;
        runCalibration();
        currentState = READY;
      }
      break;

    case READY:
      if (btnPressed && !isBatteryLowState) {
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
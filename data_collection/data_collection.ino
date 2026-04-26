#include <Wire.h>
#include <Kalman.h>


// ==================== PIN DEFINITIONS ====================
#define FLEX_1 25  
#define FLEX_2 33  
#define FLEX_3 32  
#define FLEX_4 35  
#define FLEX_5 34  

#define BUTTON_PIN 13
#define LED_R_PIN 15
#define LED_G_PIN 2
#define LED_B_PIN 0

#define SDA_PIN 21
#define SCL_PIN 22

// ==================== MPU6050 ====================
#define IMU_ADDR 0x68

const int flexPins[5] = {FLEX_1, FLEX_2, FLEX_3, FLEX_4, FLEX_5};

Kalman kalmanX;
Kalman kalmanY;

uint32_t timer = 0;
uint8_t i2cData[14];
const uint16_t I2C_TIMEOUT = 1000;

float cal_open[5] = {0};
float cal_close[5] = {0};

float acc_offset[3] = {0};
float gyro_offset[3] = {0};

// DURUM (STATE) TANIMLARI
enum State {
  IDLE,
  CALIBRATING,
  READY,
  RECORDING
};

State currentState = IDLE;

// BUTON DEBOUNCE
unsigned long lastButtonPress = 0;
bool buttonState = HIGH;      
bool lastButtonState = HIGH;

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
  uint8_t rcode = Wire.endTransmission(sendStop);
  return rcode;
}

uint8_t i2cRead(uint8_t registerAddress, uint8_t *data, uint8_t nbytes) {
  uint32_t timeOutTimer;
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(registerAddress);
  uint8_t rcode = Wire.endTransmission(false);
  if (rcode) {
    return rcode;
  }
  Wire.requestFrom((uint8_t)IMU_ADDR, nbytes, (uint8_t)true);
  for (uint8_t i = 0; i < nbytes; i++) {
    if (Wire.available())
      data[i] = Wire.read();
    else {
      timeOutTimer = micros();
      while (((micros() - timeOutTimer) < I2C_TIMEOUT) && !Wire.available());
      if (Wire.available())
        data[i] = Wire.read();
      else {
        return 5;
      }
    }
  }
  return 0;
}

void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R_PIN, r);
  digitalWrite(LED_G_PIN, g);
  digitalWrite(LED_B_PIN, b);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10); 

  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // ==== MPU6050 RAW I2C INIT ====
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000UL);

  i2cData[0] = 7;    // Sample rate = 1000Hz (8kHz/(7+1))
  i2cData[1] = 0x00; // DLPF
  i2cData[2] = 0x00; // Gyro +-250 deg/s
  i2cData[3] = 0x00; // Accel +-2g
  
  while (i2cWrite(0x19, i2cData, 4, false)); // Parametreleri yaz
  while (i2cWrite(0x6B, 0x01, true));        // Wake up MPU
  
  delay(100);

  Serial.println("MSG: Sistem Baslatildi (I2C Manual). Kalibrasyon icin Butona Basin.");
  
  setLED(0, 0, 1); // IDLE - Mavi
}

bool isButtonPressed() {
  bool reading = digitalRead(BUTTON_PIN);
  bool pressed = false;
  if(reading != lastButtonState) {
    if(reading == LOW && (millis() - lastButtonPress) > 200) {
      pressed = true;
      lastButtonPress = millis();
    }
  }
  lastButtonState = reading;
  return pressed;
}

void runCalibration() {
  // 1. AÇIK EL (SARI IŞIK)
  Serial.println("MSG: Lutfen elinizi TUMUYLE ACIK (DUZ) tutun! (2 sn bekleyin)");
  setLED(1, 1, 0); // SARI
  delay(2500); 
  Serial.println("MSG: Acik el degerleri okunuyor...");
  
  long f_sum[5] = {0,0,0,0,0};
  for(int i=0; i<20; i++) {
    for(int j=0; j<5; j++) f_sum[j] += analogRead(flexPins[j]);
    delay(20);
  }
  for(int j=0; j<5; j++) cal_open[j] = f_sum[j] / 20.0;
  
  // 2. KAPALI EL (KIRMIZI IŞIK)
  Serial.println("MSG: Lutfen elinizi TUMUYLE KAPATIN (YUMRUK)! (2 sn bekleyin)");
  setLED(1, 0, 0); // TURUNCU/KIRMIZI
  delay(2500);
  Serial.println("MSG: Kapali el degerleri okunuyor...");
  
  for(int j=0; j<5; j++) f_sum[j] = 0;
  for(int i=0; i<20; i++) {
    for(int j=0; j<5; j++) f_sum[j] += analogRead(flexPins[j]);
    delay(20);
  }
  for(int j=0; j<5; j++) cal_close[j] = f_sum[j] / 20.0;

  // 3. MPU6050 OFFSET KALİBRASYON (MOR IŞIK)
  Serial.println("MSG: Lutfen elinizi SABIT TUTUN! (Gyro/Accel kalibre ediliyor)");
  setLED(1, 0, 1); // MOR
  delay(2500);
  
  float a_sum[3] = {0};
  float g_sum[3] = {0};
  int mpu_count = 50;
  
  for(int i=0; i<mpu_count; i++) {
    while (i2cRead(0x3B, i2cData, 14)); // Okuma basarili olana kadar bekle
    int16_t accX = (i2cData[0] << 8) | i2cData[1];
    int16_t accY = (i2cData[2] << 8) | i2cData[3];
    int16_t accZ = (i2cData[4] << 8) | i2cData[5];
    int16_t gyroX = (i2cData[8] << 8) | i2cData[9];
    int16_t gyroY = (i2cData[10] << 8) | i2cData[11];
    int16_t gyroZ = (i2cData[12] << 8) | i2cData[13];
    
    a_sum[0] += accX;
    a_sum[1] += accY;
    a_sum[2] += accZ - 16384.0; // +-2g icin 1G degeri (Z ekseni kaldir)
    g_sum[0] += gyroX;
    g_sum[1] += gyroY;
    g_sum[2] += gyroZ;
    delay(10);
  }
  
  acc_offset[0] = a_sum[0] / mpu_count;
  acc_offset[1] = a_sum[1] / mpu_count;
  acc_offset[2] = a_sum[2] / mpu_count;
  
  gyro_offset[0] = g_sum[0] / mpu_count;
  gyro_offset[1] = g_sum[1] / mpu_count;
  gyro_offset[2] = g_sum[2] / mpu_count;

  // Kalman İlk Açılarını Ata
  while (i2cRead(0x3B, i2cData, 14));
  double aX = (int16_t)((i2cData[0] << 8) | i2cData[1]);
  double aY = (int16_t)((i2cData[2] << 8) | i2cData[3]);
  double aZ = (int16_t)((i2cData[4] << 8) | i2cData[5]);
  double roll  = atan2(aY, aZ) * RAD_TO_DEG;
  double pitch = atan(-aX / sqrt(aY * aY + aZ * aZ)) * RAD_TO_DEG;
  kalmanX.setAngle(roll);
  kalmanY.setAngle(pitch);
  timer = micros();

  Serial.println("MSG: KALIBRASYON TAMAMLANDI! (Hazir)");
  Serial.println("MSG: Yeni kayit icin butona basin.");
}

void recordData3Sec() {
  Serial.println("START_RECORD");
  setLED(0, 1, 1); // CYAN
  
  unsigned long startT = millis();
  
  while(millis() - startT < 3000) {
    unsigned long loopStart = millis();
    
    // Flex okuma (Eski mantik)
    float flex_norm[5];
    for(int i=0; i<5; i++) {
      float raw = analogRead(flexPins[i]);
      float diff = cal_close[i] - cal_open[i];
      if (abs(diff) < 1.0) {
         flex_norm[i] = 0;
      } else {
         flex_norm[i] = (raw - cal_open[i]) / diff;
         if(flex_norm[i] < 0.0) flex_norm[i] = 0.0;
         if(flex_norm[i] > 1.0) flex_norm[i] = 1.0;
      }
    }
    
    // MPU okuma (Raw I2C)
    while (i2cRead(0x3B, i2cData, 14)); // veriyi cek
    double accX = (int16_t)((i2cData[0] << 8) | i2cData[1]);
    double accY = (int16_t)((i2cData[2] << 8) | i2cData[3]);
    double accZ = (int16_t)((i2cData[4] << 8) | i2cData[5]);
    double gyroX = (int16_t)((i2cData[8] << 8) | i2cData[9]);
    double gyroY = (int16_t)((i2cData[10] << 8) | i2cData[11]);
    double gyroZ = (int16_t)((i2cData[12] << 8) | i2cData[13]);
    
    // Kalman hesaplama
    double dt = (double)(micros() - timer) / 1000000.0;
    timer = micros();
    
    double r = atan2(accY, accZ) * RAD_TO_DEG;
    double p = atan(-accX / sqrt(accY * accY + accZ * accZ)) * RAD_TO_DEG;
    
    double gyroXrate = gyroX / 131.0; // +- 250 deg/s mode
    double gyroYrate = gyroY / 131.0;

    float kalRoll = kalmanX.getAngle(r, gyroXrate, dt);
    float kalPitch = kalmanY.getAngle(p, gyroYrate, dt);
    
    float f_ax = accX - acc_offset[0];
    float f_ay = accY - acc_offset[1];
    float f_az = accZ - acc_offset[2]; 
    float f_gx = gyroX - gyro_offset[0];
    float f_gy = gyroY - gyro_offset[1];
    float f_gz = gyroZ - gyro_offset[2];
    
    // YAZDIR
    Serial.print("DATA,");
    for(int i=0; i<5; i++) { Serial.print(flex_norm[i], 3); Serial.print(","); }
    Serial.print(kalPitch, 2); Serial.print(",");
    Serial.print(kalRoll, 2); Serial.print(",");
    Serial.print(f_ax, 2); Serial.print(",");
    Serial.print(f_ay, 2); Serial.print(",");
    Serial.print(f_az, 2); Serial.print(",");
    Serial.print(f_gx, 2); Serial.print(",");
    Serial.print(f_gy, 2); Serial.print(",");
    Serial.print(f_gz, 2);
    Serial.println();
    
    // 50Hz sagla (20ms loop time)
    unsigned long loopEnd = millis();
    if(loopEnd - loopStart < 20) {
      delay(20 - (loopEnd - loopStart));
    }
  }
  
  Serial.println("END_RECORD");
  Serial.println("MSG: Kayit tamam. Yeni kayit icin butona basin.");
  setLED(0, 1, 0); // YESIL
}

void loop() {
  bool btnPressed = isButtonPressed();

  switch(currentState) {
    case IDLE:
      if(btnPressed) {
        currentState = CALIBRATING;
        runCalibration();
        currentState = READY;
        setLED(0, 1, 0); // Yeşil
      }
      break;

    case READY:
      if(btnPressed) {
        currentState = RECORDING;
        recordData3Sec();
        currentState = READY;
      }
      break;
        
    case CALIBRATING:
    case RECORDING:
      break;
  }
}

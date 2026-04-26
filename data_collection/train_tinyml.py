import os
import sys
import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Conv1D, MaxPooling1D, Flatten, Dense, Dropout
from sklearn.model_selection import train_test_split

def convert_h_file(tflite_path, header_path):
    with open(tflite_path, 'rb') as tflite_file:
        tflite_content = tflite_file.read()
    
    hex_lines = [', '.join([f'0x{b:02x}' for b in tflite_content[i:i+12]]) for i in range(0, len(tflite_content), 12)]
    hex_array = ',\n  '.join(hex_lines)
    
    with open(header_path, 'w') as header_file:
        header_file.write('// TFLite Model Byte Array\n\n')
        header_file.write(f'unsigned const int gesture_model_tflite_len = {len(tflite_content)};\n')
        header_file.write('const unsigned char gesture_model_tflite[] = {\n  ')
        header_file.write(f'{hex_array}\n')
        header_file.write('};\n')
        
    print(f"C++ Array olusturuldu (Boyut: ~{len(tflite_content)/1024:.2f} KB): {header_path}")

def main():
    print("="*50)
    print("   TinyML Sınıflandırma Modeli Eğitimi")
    print("="*50)

    dataset_path = 'dataset.csv'
    if not os.path.exists(dataset_path):
        print(f"HATA: '{dataset_path}' bulunamadı. Lütfen önce veri toplayın.")
        return

    # Veriyi yükle
    df = pd.read_csv(dataset_path)
    df = df.dropna()
    print(f">> Toplam okunan veri satırı: {len(df)}")
    
    feature_cols = ['flex_1', 'flex_2', 'flex_3', 'flex_4', 'flex_5',
                    'kalman_pitch', 'kalman_roll',
                    'acc_x', 'acc_y', 'acc_z', 'gyro_x', 'gyro_y', 'gyro_z']
    
    # Normalizasyon Sınırları (ESP32 icine de gommemiz gerekecek)
    # Flex (0-1000) -> / 1000
    # Kalman (-180, +180) -> / 180
    # Accel (+-2g) -> / 16384
    # Gyro (+-250) -> / 32768
    
    def normalize_row(row):
        norm = np.zeros(13, dtype=np.float32)
        norm[0:5] = row[0:5]                   # flex is already 0.0 to 1.0
        norm[5:7] = row[5:7] / 180.0           # kalman
        norm[7:10] = row[7:10] / 16384.0       # accel
        norm[10:13] = row[10:13] / 32768.0     # gyro raw max
        return norm

    # Group by record_id
    SEQ_LEN = 100 # 2 saniye kayıt * 50 Hz = 100 Adım
    
    grouped = df.groupby('record_id')
    X_raw = []
    y_raw = []
    
    unique_labels = sorted(df['label'].unique())
    label_map = {l: i for i, l in enumerate(unique_labels)}
    NUM_CLASSES = len(unique_labels)
    print(f">> Bulunan Hareketler: {unique_labels} -> Sinif Indisleri: {label_map}")

    for name, group in grouped:
        features = group[feature_cols].values
        label = group['label'].iloc[0]
        
        # Normlama islemini yap
        norm_features = np.array([normalize_row(r) for r in features])
        
        # Sequence Pad/Truncate
        if len(norm_features) > SEQ_LEN:
            norm_features = norm_features[:SEQ_LEN]
        elif len(norm_features) < SEQ_LEN:
            pad_len = SEQ_LEN - len(norm_features)
            norm_features = np.pad(norm_features, ((0, pad_len), (0, 0)), 'constant')
            
        X_raw.append(norm_features)
        y_raw.append(label_map[label])

    X = np.array(X_raw, dtype=np.float32)
    y = np.array(y_raw, dtype=np.int32)
    
    print(f">> İslenen Toplam Örneklem (Batch) Sayisi: {len(X)}")
    print(f">> Girdi Boyutu: {X.shape} - Çikti Boyutu: {y.shape}")

    # Train / Test Split
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    print("\n>> 1D-CNN Ağı Eğitimi Başlıyor...")
    model = Sequential([
        Conv1D(filters=16, kernel_size=3, activation='relu', input_shape=(SEQ_LEN, 13)),
        MaxPooling1D(pool_size=2),
        Conv1D(filters=32, kernel_size=3, activation='relu'),
        MaxPooling1D(pool_size=2),
        Flatten(),
        Dense(64, activation='relu'),
        Dropout(0.3),
        Dense(NUM_CLASSES, activation='softmax')
    ])

    model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
    
    # Eğitimi Başlat
    history = model.fit(X_train, y_train, epochs=20, batch_size=16, validation_data=(X_test, y_test), verbose=1)

    loss, accuracy = model.evaluate(X_test, y_test, verbose=0)
    print(f"\n>> Model Test % Başarısı (Accuracy): {(accuracy * 100):.2f}%")

    print("\n>> '.tflite' Dönüşümü ve Optimizasyon Başlıyor...")
    # Convert to TFLite
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    # Varsayilan (Hafif) Optimizasyon
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_model = converter.convert()
    
    tflite_file = "gesture_model.tflite"
    with open(tflite_file, "wb") as f:
        f.write(tflite_model)
    print(f"'{tflite_file}' cihazınıza kaydedildi!")
    
    print("\n>> ESP32 için C++ Başlık Dosyası 'model.h' Oluşturuluyor...")
    convert_h_file(tflite_file, "model.h")
    print("\nTebrikler! TinyML boru hattı (pipeline) başarıyla tamamlandı.")
    print("Masaüstünüzdeki 'model.h' dosyasını ESP32 kodunuza dahil ederek test eedebilirsiniz.")

if __name__ == "__main__":
    main()

import os
import glob
import numpy as np
from sklearn.tree import DecisionTreeClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import confusion_matrix, classification_report
import joblib
from micromlgen import port
from signals_esp32 import SampleESP32

print("Eğitim başlatılıyor (DecisionTree - ESP32 için optimize)...")

x_data = []
y_data = []
classes = {}
label_to_id = {}

root = r"c:\Users\berkc\Desktop\AKKE\converted_dataset"
csv_files = glob.glob(os.path.join(root, "*.csv"))

print(f"'{root}' dizininden veri seti yükleniyor...")

for file_path in csv_files:
    basename = os.path.basename(file_path).replace(".csv", "")
    category = basename.split(".")[0] # e.g., "one" from "one.1.csv"
    
    if category not in label_to_id:
        label_to_id[category] = len(label_to_id)
        classes[label_to_id[category]] = category
        
    number = label_to_id[category]
    
    # Load and process sample (resamples to 50 steps, standardizes, flattens)
    sample = SampleESP32.load_from_csv(file_path)
    if sample is not None:
        x_data.append(sample.get_linearized())
        y_data.append(number)

print(f"Veri Yükleme Tamamlandı. Toplam örnek: {len(x_data)}")
print(f"Sınıf sayısı: {len(classes)}")
print(f"Sınıflar: {classes}")

# DecisionTree - ESP32 için mükemmel: küçük model, hızlı çıkarım
clf = DecisionTreeClassifier(max_depth=10, random_state=0)

# Split the dataset
X_train, X_test, Y_train, Y_test = train_test_split(x_data, y_data, test_size=0.35, random_state=0)

print("Eğitim işlemi başlıyor...")
clf.fit(X_train, Y_train)

# Confusion Matrix
print("\nConfusion Matrix:")
Y_predicted = clf.predict(X_test)
print(confusion_matrix(Y_test, Y_predicted))

# Calculate the score
score = clf.score(X_test, Y_test)
print(f"\nTEST BAŞARISI (SCORE): {score * 100:.2f}%\n")

print("Model kaydediliyor...")
joblib.dump(clf, 'model_esp32.pkl')
joblib.dump(classes, 'classes_esp32.pkl')

print("ESP32 için C++ kütüphanesi oluşturuluyor (model.h)...")
try:
    c_code = port(clf, classmap=classes)
    with open(r"c:\Users\berkc\Desktop\AKKE\inference\model.h", "w") as f:
        f.write(c_code)
    print("ESP32 header dosyası başarıyla 'inference/model.h' yoluna kaydedildi!")
    
    # Dosya boyutunu kontrol et
    file_size = os.path.getsize(r"c:\Users\berkc\Desktop\AKKE\inference\model.h")
    print(f"model.h dosya boyutu: {file_size / 1024:.1f} KB")
except Exception as e:
    print("Micromlgen dışa aktarımında bir hata oluştu:", e)

print("TÜM İŞLEMLER BAŞARIYLA TAMAMLANDI!")

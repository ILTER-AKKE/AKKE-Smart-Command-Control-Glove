import os
from collections import Counter

data_dir = "edge_impulse_data"

if not os.path.exists(data_dir):
    print(f"Hata: '{data_dir}' klasörü bulunamadı!")
    exit()

label_counts = Counter()

# Klasördeki dosyaları oku
for filename in os.listdir(data_dir):
    if filename.endswith(".csv"):
        # Dosya isimleri genelde "Hareket_X.timestamp.csv" formatında
        parts = filename.split(".")
        if len(parts) >= 2:
            label = parts[0]
            label_counts[label] += 1

# Sonuçları ekrana yazdır
print("==================================================")
print("   Edge Impulse Klasörü Etiket (Label) Sayıları   ")
print("==================================================")

if not label_counts:
    print("Klasörde hiç CSV dosyası bulunamadı.")
else:
    # Sayıları hareket numarasına göre sıralayarak yazdır (mümkünse)
    # Etiket isminden rakamı çıkarmaya çalışıp sıralıyoruz
    def sort_key(x):
        label = x[0]
        # "Hareket_10" gibi isimlerden sadece sayıyı alıp sayısal sıralama yapalım
        if "_" in label:
            num = label.split("_")[-1]
            if num.isdigit():
                return int(num)
        return label
    
    for label, count in sorted(label_counts.items(), key=sort_key):
        print(f"{label:<15} : {count} adet")

print("--------------------------------------------------")
total = sum(label_counts.values())
print(f"Toplam CSV Sayısı : {total}")
print("==================================================")

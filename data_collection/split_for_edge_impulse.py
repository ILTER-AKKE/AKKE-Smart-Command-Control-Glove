import csv
import os

def main():
    input_file = "dataset.csv"
    output_dir = "edge_impulse_data"
    
    if not os.path.exists(input_file):
        print(f"Hata: {input_file} bulunamadi.")
        return

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print(f"{input_file} okunuyor ve Edge Impulse formatina (ayri dosyalara) bolunuyor...")

    with open(input_file, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        header = next(reader)
        
        # Indisleri bulalim
        # Beklenen: record_id,timestamp_ms,label,flex_1,flex_2,...
        try:
            record_id_idx = header.index("record_id")
            timestamp_idx = header.index("timestamp_ms")
            label_idx = header.index("label")
        except ValueError:
            print("Hata: Gerekli basliklar (record_id, timestamp_ms, label) bulunamadi.")
            return

        # Feature isimlerini cikar (ilk 3 sutun haric)
        feature_names = header[3:]
        # Yeni baslik: timestamp, feature1, feature2...
        out_header = ["timestamp"] + feature_names

        records = {}
        for row in reader:
            if not row or len(row) < len(header): 
                continue # bos veya eksik satirlari atla
            
            rec_id = row[record_id_idx]
            lbl = row[label_idx]
            ts = int(row[timestamp_idx])
            features = row[3:]
            
            if rec_id not in records:
                records[rec_id] = {
                    "label": lbl,
                    "start_ts": ts,
                    "rows": []
                }
            
            # timestamp 0'dan baslasin diye relative yapiyoruz
            rel_ts = ts - records[rec_id]["start_ts"]
            out_row = [rel_ts] + features
            records[rec_id]["rows"].append(out_row)

    # Simdi klasore yazdiriyoruz
    # Dosya adlandirmasi: Label.RecordID.csv (Ornek: 1.1776358307862.csv)
    # Edge impulse "Infer from filename" secersek "1" kismini otomatik etiket sayar!
    
    file_count = 0
    for rec_id, data in records.items():
        lbl = data["label"]
        # Edge impulse icin dosya ismi formulu: etiketismi.benzersizID.csv
        out_filename = f"Hareket_{lbl}.{rec_id}.csv"
        out_path = os.path.join(output_dir, out_filename)
        
        with open(out_path, 'w', newline='', encoding='utf-8') as out_f:
            writer = csv.writer(out_f)
            writer.writerow(out_header)
            writer.writerows(data["rows"])
            
        file_count += 1

    print(f"ISLEM TAMAM! Toplam {file_count} farkli hareket dosyasi '{output_dir}' klasorune cikarildi.")
    print("Edge Impulse sitesine bu klasorun ICINDEKI tum dosyalari secerek yukleyebilirsiniz.")

if __name__ == "__main__":
    main()

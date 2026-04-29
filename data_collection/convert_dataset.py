import os
import csv
import glob

# Paths
input_dir = r"c:\Users\berkc\Downloads\Gesture-Recognition-master\Gesture-Recognition-master\data\data"
output_dir = r"c:\Users\berkc\Desktop\AKKE\converted_dataset"

os.makedirs(output_dir, exist_ok=True)

txt_files = glob.glob(os.path.join(input_dir, "*.txt"))

# Edge Impulse expects headers for time-series data
headers = ["timestamp", "flex_1", "flex_2", "flex_3", "flex_4", "flex_5", "kalman_pitch", "kalman_roll", "acc_x", "acc_y", "acc_z", "gyro_x", "gyro_y", "gyro_z"]

file_counters = {}
total_files_processed = 0

for file_path in txt_files:
    basename = os.path.basename(file_path).replace(".txt", "")
    
    # Extract clean label
    label = basename.replace("new_", "").replace("old", "").rstrip("0123456789")
    
    # Skip general or corrupted data files
    if label == "data" or label == "test" or label == "":
        continue
    
    if label not in file_counters:
        file_counters[label] = 1
    else:
        file_counters[label] += 1
        
    out_filename = f"{label}.{file_counters[label]}.csv"
    out_path = os.path.join(output_dir, out_filename)
    
    valid_rows = 0
    with open(file_path, "r") as fin, open(out_path, "w", newline='') as fout:
        writer = csv.writer(fout)
        writer.writerow(headers)
        
        # Read lines
        lines = fin.readlines()
        timestamp = 0
        for line in lines:
            parts = line.strip().split()
            if len(parts) >= 16:
                try:
                    # 0-4: 5 Flex Sensors (keeping)
                    # 5-6: 2 Extra Flex Sensors (Wrist/Arm) -> SKIPPING
                    # 7-8: Kalman Pitch, Roll (keeping)
                    # 9: Kalman Yaw -> SKIPPING
                    # 10-12: Acc X, Y, Z (keeping)
                    # 13-15: Gyro X, Y, Z (keeping)
                    
                    f1, f2, f3, f4, f5 = parts[0:5]
                    pitch, roll = parts[7:9]
                    ax, ay, az = parts[10:13]
                    gx, gy, gz = parts[13:16]
                    
                    row = [timestamp, f1, f2, f3, f4, f5, pitch, roll, ax, ay, az, gx, gy, gz]
                    writer.writerow(row)
                    
                    timestamp += 10 # Assuming 100Hz = 10ms intervals
                    valid_rows += 1
                except ValueError:
                    pass
    
    if valid_rows > 0:
        total_files_processed += 1
    else:
        # Remove empty files
        os.remove(out_path)

print(f"Veri donusumu tamamlandi!")
print(f"Toplam islenen dosya (label) sayisi: {total_files_processed}")
print(f"Olusturulan CSV dosyalari '{output_dir}' klasorune kaydedildi.")
print("Bu dosyalari Edge Impulse 'Data acquisition' kismindan 'Upload existing data' diyerek yukleyebilirsiniz.")

import serial
import serial.tools.list_ports
import time
import csv
import os

def list_serial_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("Uyari: Bagli bir seri port bulunamadi.")
        return []
    print("\nMevcut Seri Portlar:")
    for i, port in enumerate(ports):
        print(f"[{i}] {port.device} - {port.description}")
    return ports

def main():
    print("="*50)
    print("   Akilli Eldiven Veri Toplayici (TinyML Icin)")
    print("="*50)

    ports = list_serial_ports()
    if not ports:
        port_name = input("Lutfen manuel port adini girin (ornegin COM3 veya /dev/ttyUSB0): ")
    else:
        sel = input("\nLutfen bir port secin (0, 1, vs.) veya manuel girmek icin 'm' yazin: ")
        if sel.lower() == 'm':
            port_name = input("Manuel port adini girin: ")
        else:
            try:
                port_name = ports[int(sel)].device
            except:
                print("Gecersiz secim. Cikiliyor.")
                return

    baud_rate = 115200

    # Etiket Seçimi
    print("\n" + "="*50)
    print("Hangi hareket icin veri toplayacaksiniz?")
    print("Ornegin: 1 (Hareket 1), 2 (Hareket 2), 3 (Hareket 3)")
    label_input = input("Hareket Etiketi (Tamsayi): ")
    
    try:
        label = int(label_input)
    except ValueError:
        print("Lutfen gecerli bir tamsayi girin. Varsayilan '0' atandi.")
        label = 0

    dataset_filename = "dataset.csv"
    file_exists = os.path.isfile(dataset_filename)

    with open(dataset_filename, mode='a', newline='') as f:
        writer = csv.writer(f)
        # Header yaz (Eğer dosya yeni ise)
        if not file_exists:
            writer.writerow(["record_id", "timestamp_ms", "label", 
                             "flex_1", "flex_2", "flex_3", "flex_4", "flex_5",
                             "kalman_pitch", "kalman_roll",
                             "acc_x", "acc_y", "acc_z", "gyro_x", "gyro_y", "gyro_z"])

    print(f"\n[{port_name}] portuna {baud_rate} baud ile baglaniliyor...")
    
    try:
        ser = serial.Serial(port_name, baud_rate, timeout=1)
        time.sleep(2) # Reset sonrasi ESP32'nin boot olmasini bekle
    except Exception as e:
        print(f"Baglanti hatasi: {e}")
        return

    print("\nBAŞARILI! ESP32 dinleniyor...")
    print("Kalibrasyon icin Cihazdaki BUTONA (1. kez) basin.")
    print("Kayit almak icin butona (2. kez ve sonrasinda) basin.\n")

    recording = False
    record_id = 0
    current_record_data = []

    # Eğer daha önce bu cihazda kaydedilmiş record_id varsa onu bulmak akıllıca olabilir
    # Ancak sıfırdan her sessionda artması da yeterlidir.
    # Biz basitçe session bazlı record_id sayacağız. Toplam kaç kayıt aldığını göstersin.
    session_record_count = 0 
    target_records = 400

    try:
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                # Sadece mesajları consola basalım
                if line.startswith("MSG:"):
                    print(f"\n[CİHAZ] {line[4:]}")
                
                elif line == "START_RECORD":
                    recording = True
                    current_record_data = []
                    record_id = int(time.time() * 1000) # Benzersiz record ID epoch
                    print(f"\n--- 3 Saniyelik Kayit Basladi (Etiket: {label}) ---")
                
                elif line == "END_RECORD":
                    recording = False
                    session_record_count += 1
                    
                    # Veriyi CSV'ye kaydet
                    with open(dataset_filename, mode='a', newline='') as f:
                        writer = csv.writer(f)
                        for row in current_record_data:
                            writer.writerow(row)
                            
                    print(f"--- Kayit Bitti ve Kaydedildi! ---")
                    print(f"Bugun Bu Oturumda Alinan Toplam Kayit: {session_record_count} / {target_records}")
                    
                    if session_record_count >= target_records:
                        print(f"\n[{target_records}/{target_records}] Hedeflenen kayit sayisina ulasildi! Program sonlandiriliyor...")
                        ser.close()
                        return
                    
                elif line.startswith("DATA,"):
                    if recording:
                        # Beklenen format: DATA, f1, f2, f3, f4, f5, kalPitch, kalRoll, ax, ay, az, gx, gy, gz
                        parts = line.split(',')
                        if len(parts) == 14: # 1 DATA + 5 flex + 2 kalman + 6 imu = 14
                            timestamp_ms = int(time.time() * 1000)
                            try:
                                f1, f2, f3, f4, f5 = map(float, parts[1:6])
                                kalPitch, kalRoll = map(float, parts[6:8])
                                ax, ay, az, gx, gy, gz = map(float, parts[8:14])
                                
                                row = [record_id, timestamp_ms, label,
                                       f1, f2, f3, f4, f5,
                                       kalPitch, kalRoll,
                                       ax, ay, az, gx, gy, gz]
                                current_record_data.append(row)
                            except ValueError:
                                pass # Eger anlik bozuk veri gelirse ignore et
                                
                else:
                    # Diğer cihaz printleri
                    if "ERR" in line:
                        print(f"[HATA]: {line}")

            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n\nCikis yapildi. Baglanti kapatiliyor...")
        ser.close()

if __name__ == "__main__":
    main()

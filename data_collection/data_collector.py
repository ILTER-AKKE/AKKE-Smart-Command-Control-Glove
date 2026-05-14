import serial
import serial.tools.list_ports
import time
import csv
import os

def list_serial_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("Warning: No serial port found.")
        return []
    print("\nAvailable Serial Ports:")
    for i, port in enumerate(ports):
        print(f"[{i}] {port.device} - {port.description}")
    return ports

def main():
    print("="*50)
    print("   Smart Glove Data Collector")
    print("="*50)

    ports = list_serial_ports()
    if not ports:
        port_name = input("Please enter the serial port manually (e.g., COM3 or /dev/ttyUSB0): ")
    else:
        sel = input("\nPlease select a port (0, 1, etc.) or type 'm' to enter manually: ")
        if sel.lower() == 'm':
            port_name = input("Enter the serial port name: ")
        else:
            try:
                port_name = ports[int(sel)].device
            except:
                print("Invalid selection. Exiting.")
                return

    baud_rate = 115200

    # Label Selection
    print("\n" + "="*50)
    print("For which movement are you collecting data?")
    print("For example: 1 (Movement 1), 2 (Movement 2), 3 (Movement 3)")
    label_input = input("Movement Label (Integer): ")
    
    try:
        label = int(label_input)
    except ValueError:
        print("Please enter a valid integer. Default '0' assigned.")
        label = 0

    dataset_filename = "dataset.csv"
    file_exists = os.path.isfile(dataset_filename)

    with open(dataset_filename, mode='a', newline='') as f:
        writer = csv.writer(f)
        # Target File Header
        if not file_exists:
            writer.writerow(["record_id", "timestamp_ms", "label", 
                             "flex_1", "flex_2", "flex_3", "flex_4", "flex_5",
                             "kalman_pitch", "kalman_roll",
                             "acc_x", "acc_y", "acc_z", "gyro_x", "gyro_y", "gyro_z"])

    print(f"\nConnecting to {port_name} at {baud_rate} baud...")
    
    try:
        ser = serial.Serial(port_name, baud_rate, timeout=1)
        time.sleep(2) # Wait for ESP32 to boot after reset
    except Exception as e:
        print(f"Connection error: {e}")
        return

    print("\nSUCCESS! ESP32 is listening...")
    print("Press the button on the device (1st time) for calibration.")
    print("Press the button (2nd time and onwards) to record.\n")

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
                    print(f"\n[DEVICE] {line[4:]}")
                
                elif line == "START_RECORD":
                    recording = True
                    current_record_data = []
                    record_id = int(time.time() * 1000) # Unique record ID epoch
                    print(f"\n--- 3 Second Recording Started (Label: {label}) ---")
                
                elif line == "END_RECORD":
                    recording = False
                    session_record_count += 1
                    
                    # Veriyi CSV'ye kaydet
                    with open(dataset_filename, mode='a', newline='') as f:
                        writer = csv.writer(f)
                        for row in current_record_data:
                            writer.writerow(row)
                            
                    print(f"--- Recording Finished and Saved! ---")
                    print(f"Total Records Collected in This Session: {session_record_count} / {target_records}")
                    
                    if session_record_count >= target_records:
                        print(f"\n[{target_records}/{target_records}] Target number of records reached! Program terminating...")
                        ser.close()
                        return
                    
                elif line.startswith("DATA,"):
                    if recording:
                        # Expected format: DATA, f1, f2, f3, f4, f5, kalPitch, kalRoll, ax, ay, az, gx, gy, gz
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
                                pass # If corrupt data is received, ignore it
                                
                else:
                    # Other device prints
                    if "ERR" in line:
                        print(f"[ERROR]: {line}")

            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n\nExiting. Closing connection...")
        ser.close()

if __name__ == "__main__":
    main()

import csv
import os

def main():
    input_file = "dataset.csv"
    output_dir = "edge_impulse_data"
    
    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found.")
        return

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    print(f"{input_file} reading and splitting into individual files for Edge Impulse format...")

    with open(input_file, 'r', encoding='utf-8') as f:
        reader = csv.reader(f)
        header = next(reader)
        
        # Expected: record_id,timestamp_ms,label,flex_1,flex_2,...
        try:
            record_id_idx = header.index("record_id")
            timestamp_idx = header.index("timestamp_ms")
            label_idx = header.index("label")
        except ValueError:
            print("Error: Required headers (record_id, timestamp_ms, label) not found.")
            return

        # Feature names
        feature_names = header[3:]
        out_header = ["timestamp"] + feature_names

        records = {}
        for row in reader:
            if not row or len(row) < len(header): 
                continue # skip empty or incomplete rows
            
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
            
            # Relative timestamp
            rel_ts = ts - records[rec_id]["start_ts"]
            out_row = [rel_ts] + features
            records[rec_id]["rows"].append(out_row)

    # Writing to folder
    # File naming: Label.RecordID.csv (Example: 1.1776358307862.csv)
    # If Edge impulse "Infer from filename" is selected, it will automatically label "1"!
    
    file_count = 0
    for rec_id, data in records.items():
        lbl = data["label"]
        # File name formula for Edge Impulse: tagName.uniqueID.csv
        out_filename = f"Hareket_{lbl}.{rec_id}.csv"
        out_path = os.path.join(output_dir, out_filename)
        
        with open(out_path, 'w', newline='', encoding='utf-8') as out_f:
            writer = csv.writer(out_f)
            writer.writerow(out_header)
            writer.writerows(data["rows"])
            
        file_count += 1

    print(f"DONE! {file_count} different movement files extracted to '{output_dir}' folder.")
    print("You can upload all files inside this folder to Edge Impulse site.")

if __name__ == "__main__":
    main()

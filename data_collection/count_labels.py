import os
from collections import Counter

data_dir = "edge_impulse_data"

if not os.path.exists(data_dir):
    print(f"Error: '{data_dir}' folder not found!")
    exit()

label_counts = Counter()

# Read files in the folder
for filename in os.listdir(data_dir):
    if filename.endswith(".csv"):
        # File names are generally in the "Hareket_X.timestamp.csv" format
        parts = filename.split(".")
        if len(parts) >= 2:
            label = parts[0]
            label_counts[label] += 1

# Print results to the screen
print("==================================================")
print("   Edge Impulse Folder Label (Label) Counts   ")
print("==================================================")

if not label_counts:
    print("No CSV file found in the folder.")
else:
    # Print the numbers in the order of the move number (if possible)
    # Try to extract the number from the label name and sort numerically
    def sort_key(x):
        label = x[0]
        # "Hareket_10" like names, extract only the number and sort numerically
        if "_" in label:
            num = label.split("_")[-1]
            if num.isdigit():
                return int(num)
        return label
    
    for label, count in sorted(label_counts.items(), key=sort_key):
        print(f"{label:<15} : {count} adet")

print("--------------------------------------------------")
total = sum(label_counts.values())
print(f"Total CSV Number : {total}")
print("==================================================")

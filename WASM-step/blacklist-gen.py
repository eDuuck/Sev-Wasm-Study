import pandas as pd
import struct
import sys

def filter_frequent_pages(csv_path, threshold=1, track_modes=('a', 'w', 'e')):
    df = pd.read_csv(csv_path)
    filtered_df = df[df['Track Mode'].isin(track_modes)]
    counts = filtered_df['Page Access'].value_counts()
    frequent_pages = counts[counts > threshold].index.tolist()
    return sorted(frequent_pages), counts.size

def save_pages_to_binary(pages, output_path):
    with open(output_path, 'wb') as f:
        for page in pages:
            f.write(struct.pack('<Q', page))  # Little-endian unsigned 64-bit

def save_pages_to_text(pages, output_path):
    with open(output_path, 'w') as f:
        for page in pages:
            f.write(f"{page}\n")

# === USAGE ===
if __name__ == "__main__":
    file_id = "25143002"
    csv_file = sys.path[0][:-10] + "/output/page-track-" + file_id + ".csv"
    min_threshold = 1
    allowed_modes = ('a','w','e')  # Can be ('a',), ('w', 'e'), etc.

    result,input_size = filter_frequent_pages(csv_file, threshold=1, track_modes=allowed_modes)

    # Write as binary (C-friendly)
    save_pages_to_binary(result, "output/frequent_pages.bin")

    # Optional: Write as text
    save_pages_to_text(result, "output/frequent_pages.txt")

    print(f"Wrote {len(result)} filtered page addresses out of {input_size} to file.")

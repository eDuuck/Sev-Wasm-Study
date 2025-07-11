import re
from collections import defaultdict

# --- Config ---
input_file = "output/pagefaults.log"  # Or replace with a multiline string if needed
pattern_min_len = 2
pattern_max_len = 10
repeat_threshold = 10
min_unique_gpas_in_pattern = 6  # ← or however many you want

def extract_gpas(lines):
    hex_pattern = re.compile(r'GPA:0x([0-9a-fA-F]+)')
    return [int(m.group(1), 16) for line in lines if (m := hex_pattern.search(line))]

def is_repetition_of_smaller(pattern):
    for i in range(1, len(pattern)):
        unit = pattern[:i]
        if len(pattern) % i == 0:
            if unit * (len(pattern) // i) == pattern:
                return True
    return False

def find_repeating_patterns(seq, min_len, max_len, threshold, min_unique):
    patterns = defaultdict(int)
    n = len(seq)
    for l in range(min_len, min(max_len + 1, n)):
        for i in range(n - l + 1):
            sub = tuple(seq[i:i + l])
            if len(set(sub)) < min_unique:
                continue  # Skip boring patterns
            patterns[sub] += 1
    return {p: c for p, c in patterns.items() if c >= threshold}


def filter_non_redundant(patterns):
    # Remove patterns that are full repetitions of smaller ones
    return {
        p: c for p, c in patterns.items()
        if not is_repetition_of_smaller(p)
    }

def filter_minimal_repeating_patterns(patterns):
    # First, remove exact repetitions of smaller units (as before)
    filtered = {}
    for p, count in patterns.items():
        if not is_repetition_of_smaller(p):
            filtered[p] = count

    # Now remove supersets or shifts of already included patterns
    minimal = {}
    for p1, c1 in sorted(filtered.items(), key=lambda x: (-x[1], len(x[0]))):  # Prefer frequent and short
        is_subpattern = False
        for p2 in minimal:
            for i in range(len(p2) - len(p1) + 1):
                if p2[i:i+len(p1)] == p1:
                    is_subpattern = True
                    break
            if is_subpattern:
                break
        if not is_subpattern:
            minimal[p1] = c1
    return minimal

if __name__ == "__main__":
    with open(input_file) as f:
        gpas = extract_gpas(f.readlines())

    print(f"Found {len(gpas)} GPA entries. Scanning for patterns...")

    raw_results = find_repeating_patterns(
    gpas,
    pattern_min_len,
    pattern_max_len,
    repeat_threshold,
    min_unique_gpas_in_pattern
)
    filtered_results = filter_minimal_repeating_patterns(raw_results)

    if not filtered_results:
        print("No unique repeating patterns found.")
    else:
        print(f"\nTop patterns (primitive only):")
        for pattern, count in sorted(filtered_results.items(), key=lambda x: -x[1]):
            pretty = ' → '.join(f"0x{x:x}" for x in pattern)
            print(f"{count}x: {pretty}")
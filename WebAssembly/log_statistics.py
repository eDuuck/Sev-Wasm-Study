import os
from collections import Counter

def print_histogram(counter, top_n=20, width=40):
    """Print an ASCII histogram for the most common opcodes."""
    most = counter.most_common(top_n)
    if not most:
        print("  (no opcodes)")
        return

    max_count = most[0][1]

    print(f"  Histogram (top {top_n} opcodes):")
    for op, count in most:
        bar_len = int((count / max_count) * width)
        bar = "█" * bar_len
        print(f"    {op:>3} {bar:<{width}} {count:,}")
    print()

def parse_wasm_file(path):
    """
    Parse one WebAssembly trace file.
    Returns:
        (opcode_counter, performed_insts)
    """
    opcode_counter = Counter()
    performed = None

    with open(path, "r") as f:
        for line in f:
            line = line.strip()

            # Extract "Performed instructions"
            if line.startswith("Performed instructions"):
                # Format: "Performed instructions: XXXXX"
                performed = int(line.split(":")[1].strip())
                continue

            # Otherwise: opcode list line
            # Example: 23,c9,41,c9,...
            # Avoid loading entire line into memory if it's huge
            parts = line.split(",")
            for p in parts:
                if p:
                    opcode_counter[p] += 1

    return opcode_counter, performed


def scan_directory(root):
    """
    Walk all files in directory `root`,
    parse them, and print per-file + global statistics.
    """
    global_counter = Counter()
    total_performed = 0

    for dirpath, _, filenames in os.walk(root):
        for fn in filenames:
            fullpath = os.path.join(dirpath, fn)
            print(f"Processing {fullpath}...")

            opcode_counter, performed = parse_wasm_file(fullpath)

            # Print per-file stats
            print(f"  Performed instructions: {performed}")
            print(f"  Unique opcodes: {len(opcode_counter)}")
            top20 = opcode_counter.most_common(20)
            print(f"  Top 20 opcodes: {top20}")
            print(f"  Calls: {opcode_counter.get('10', 0)}")
            print_histogram(opcode_counter, top_n=20, width=40)
            print()

            # Accumulate global stats
            global_counter.update(opcode_counter)
            if performed is not None:
                total_performed += performed

    # Print final global stats
    print("====== GLOBAL STATISTICS ======")
    print(f"Total performed instructions: {total_performed}")
    print(f"Total unique opcodes: {len(global_counter)}")
    print("Top 20 opcodes:")
    for op, count in global_counter.most_common(20):
        print(f"  {op}: {count}")
    print(f"Calls: {global_counter.get('10', 0)}")
    print(f"Total opcodes counted: {sum(global_counter.values())}")
    print("================================")


if __name__ == "__main__":
    # CHANGE THIS to your directory path:
    scan_directory("/home/markus/sev-study/sev-study-userland/WebAssembly/logs")
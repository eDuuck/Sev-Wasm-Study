#!/bin/bash
# merge_parts.sh
# Usage: ./merge_parts.sh output/my_measurement

base=$1
output="${base}.csv"

if [ -z "$base" ]; then
  echo "Usage: $0 <base_filename_without_part_suffix>"
  exit 1
fi

echo "Merging all parts into $output..."

cat ${base}.part* > "$output"

echo "Done! Combined file: $output"

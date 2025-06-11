#!/bin/bash

# Set the correct path to the benchmark executable
BENCHMARK_BIN="./build/benchmarks/real_bitmaps_benchmark"

# Set the root dataset directory
DATASET_DIR="benchmarks/realdata"

# Output CSV file
CSV_FILE="benchmark_results.csv"

# Write CSV header
echo "dataset,encoding,compression_ratio,compression_cycles_per_value,decompression_cycles_per_value" > "$CSV_FILE"

# Print table header to terminal
printf "%-30s %-10s %-20s %-30s %-30s\n" "Dataset" "Encoding" "Compression Ratio" "Compression Cycles/Value" "Decompression Cycles/Value"

# Process each dataset
for DIR in "$DATASET_DIR"/*/; do
    if [ -d "$DIR" ]; then
        OUTPUT=$($BENCHMARK_BIN "$DIR" 2>/dev/null)
        
        # Extract fields from the known format
        COMPRATIO=$(echo "$OUTPUT" | grep "Compression_ratio" | awk '{print $2}')
        COMPTIME=$(echo "$OUTPUT" | grep "Compression_time" | awk '{print $2}')
        DECOMPTIME=$(echo "$OUTPUT" | grep "Decompression_time" | awk '{print $2}')
        DATASET_NAME=$(basename "$DIR")

        # Print to terminal
        printf "%-30s %-10s %-20s %-30s %-30s\n" "$DATASET_NAME" "roaring" "$COMPRATIO" "$COMPTIME" "$DECOMPTIME"

        # Append to CSV
        echo "$DATASET_NAME,roaring,$COMPRATIO,$COMPTIME,$DECOMPTIME" >> "$CSV_FILE"
    fi
done


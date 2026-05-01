#!/bin/bash
# Script to create a multi-frame Zstd file for Parallel Decompression

# Usage: ./make_parallel_zstd.sh [level] [frame_count] [out_dir]
# Example: ./make_parallel_zstd.sh 3 32 out/evergreen-arm64_gold

LEVEL=${1:-3}
FRAME_COUNT=${2:-32}
OUT_DIR=${3:-out/evergreen-arm-hardfp-rdk_gold}

INPUT_SO="$OUT_DIR/libcobalt.so"
OUTPUT_ZST="$OUT_DIR/libcobalt${LEVEL}x${FRAME_COUNT}.zst"

if [ ! -f "$INPUT_SO" ]; then
    echo "Error: $INPUT_SO not found."
    exit 1
fi

echo "Configuration:"
echo "  Out Dir: $OUT_DIR"
echo "  Level:   $LEVEL"
echo "  Frames:  $FRAME_COUNT"
echo "  Output:  $OUTPUT_ZST"

DIR=$(dirname "$INPUT_SO")
PREFIX="$DIR/cobalt_chunk_"

# 1. Split the library into chunks
echo "Splitting $INPUT_SO into $FRAME_COUNT parts..."
split -n $FRAME_COUNT -d "$INPUT_SO" "$PREFIX"

# 2. Compress each chunk individually
echo "Compressing chunks at level $LEVEL..."
for f in ${PREFIX}*; do
    if [[ $f == *.zst ]]; then continue; fi
    zstd -$LEVEL "$f" -o "$f.zst" --quiet
done

# 3. Concatenate them together into one file
echo "Concatenating into $OUTPUT_ZST..."
cat ${PREFIX}*.zst > "$OUTPUT_ZST"

# 4. Cleanup
rm ${PREFIX}*

echo "Success! Created $OUTPUT_ZST"

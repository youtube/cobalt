#!/bin/bash -x

set -x

# echo "artifact dir structure"
# ls -d "$artifact_dir"/*/*/*/
for artifact_dir in size_artifacts/size_artifacts_*; do
  if [[ -d "$artifact_dir" ]]; then
    # Extract platform from directory name like size_artifacts_evergreen-arm-hardfp-raspi_gold
    PLATFORM=$(basename "$artifact_dir" | sed -n 's/size_artifacts_\(.*\)_gold/\1/p')
# for artifact_dir in out/evergreen-arm-hardfp-rdk_gold/; do
#   if [[ -d "$artifact_dir" ]]; then
#     # Extract platform from directory name like size_artifacts_evergreen-arm-hardfp-raspi_gold
#     PLATFORM=evergreen-arm-hardfp-rdk
    CONFIG=gold

    if [[ -z "$PLATFORM" ]]; then
      echo "Could not extract platform from $artifact_dir"
      continue
    fi

    echo "Processing $PLATFORM"
    TARGET_DIR="out/${PLATFORM}_${CONFIG}"
    echo "target_dir : $TARGET_DIR"
    mkdir -p "${TARGET_DIR}/bin"
    mkdir -p "${TARGET_DIR}/lib"
    mkdir -p "${TARGET_DIR}/sizes"

    # Find and move libcobalt.so
    find_file() {
      local file_name="$1"
      local search_dir="$2"
      local result_var="$3"
      local file_path
      file_path=$(find "$search_dir" -name "$file_name" -type f | head -n 1)
      if [[ -z "$file_path" ]]; then
        echo "$file_name not found in $search_dir"
        return 1
      fi
      eval "$result_var"="'$file_path'"
    }

    find_file "libcobalt.so" "$artifact_dir" "libcobalt_so_src" || continue
    find_file "run_cobalt_sizes" "$artifact_dir" "RUNNER_PATH" || continue

    echo "Running Check binary size for $PLATFORM"
    python3 $RUNNER_PATH

    find_file "perf_results.json" "$artifact_dir" "SIZES_PATH" || continue


    echo "Running Compare binary size for $PLATFORM"
    python3 cobalt/testing/tools/compare_sizes.py \
      --current "$SIZES_PATH" \
      --reference cobalt/testing/tools/reference_metrics.json \
      --platform "${PLATFORM}" \
      --config "${CONFIG}"
  else
    echo "Not a directory: $artifact_dir"
  fi
done

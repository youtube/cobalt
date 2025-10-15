#!/bin/bash -x

set -x

# echo "artifact dir structure"
# ls -d "$artifact_dir"/*/*/*/
# for artifact_dir in size_artifacts/size_artifacts_*; do
#   if [[ -d "$artifact_dir" ]]; then
#     # Extract platform from directory name like size_artifacts_evergreen-arm-hardfp-raspi_gold
#     PLATFORM=$(basename "$artifact_dir" | sed -n 's/size_artifacts_\(.*\)_gold/\1/p')
for artifact_dir in out/evergreen-arm-hardfp-rdk_gold/; do
  if [[ -d "$artifact_dir" ]]; then
    # Extract platform from directory name like size_artifacts_evergreen-arm-hardfp-raspi_gold
    PLATFORM=evergreen-arm-hardfp-rdk
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
    libcobalt_so_src=$(find "$artifact_dir" -name libcobalt.so -type f | head -n 1)
    if [[ -f "$libcobalt_so_src" ]]; then
      mv "$libcobalt_so_src" "${TARGET_DIR}/lib/"
    else
      echo "libcobalt.so not found in $artifact_dir"
      continue
    fi

    RUNNER_PATH=$(find "$artifact_dir" -name run_cobalt_sizes -type f | head -n 1)
    if [[ -z "$RUNNER_PATH" ]]; then
      echo "run_cobalt_sizes not found in $artifact_dir"
      continue
    fi
    SIZES_PATH="${TARGET_DIR}/sizes/perf_results.json"

    echo "Running Check binary size for $PLATFORM"
    chmod +x "$RUNNER_PATH"
    (cd "$TARGET_DIR" && ./bin/run_cobalt_sizes > "${SIZES_PATH}")

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

#!/bin/bash -x

set -x

echo "home dir structure"
ls -d */*/*/*/

echo "artifact dir structure"
ls -d "$artifact_dir"/*/*/*/
for artifact_dir in size_artifacts/size_artifacts_*; do
  if [[ -d "$artifact_dir" ]]; then
    # Extract platform from directory name like size_artifacts_evergreen-arm-hardfp-raspi_gold
    PLATFORM=$(basename "$artifact_dir" | sed -n 's/size_artifacts_\(.*\)_gold/\1/p')
    CONFIG=gold

    if [[ -z "$PLATFORM" ]]; then
      echo "Could not extract platform from $artifact_dir"
      continue
    fi

    echo "Processing $PLATFORM"
    TARGET_DIR="out/${PLATFORM}_${CONFIG}"
    mkdir -p "${TARGET_DIR}/bin"
    mkdir -p "${TARGET_DIR}/sizes"

    # Move files to their expected locations
    if [[ -f "$artifact_dir/bin/run_cobalt_sizes" ]]; then
      mv "$artifact_dir/bin/run_cobalt_sizes" "${TARGET_DIR}/bin/"
    else
      echo "run_cobalt_sizes not found in $artifact_dir/bin"
      continue
    fi

    if [[ -d "$artifact_dir/sizes" ]]; then
      mv "$artifact_dir/sizes/"* "${TARGET_DIR}/sizes/"
    elif [[ -f "$artifact_dir/perf_results.json" ]]; then # Check if sizes was flattened
        mv "$artifact_dir/perf_results.json" "${TARGET_DIR}/sizes/"
    else
      echo "sizes directory or perf_results.json not found in $artifact_dir"
      continue
    fi

    RUNNER_PATH="${TARGET_DIR}/bin/run_cobalt_sizes"
    SIZES_PATH="${TARGET_DIR}/sizes/perf_results.json"

    echo "Running Check binary size for $PLATFORM"
    chmod +x "$RUNNER_PATH"
    "$RUNNER_PATH"

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

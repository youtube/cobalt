#!/bin/bash -x

set -x

artifact_dir=$(find . -type d -name "size_artifacts_*" | head -n 1)
if [[ -d "$artifact_dir" ]]; then
  # Extract platform from directory name like size_artifacts_evergreen-arm-hardfp-raspi_gold
  PLATFORM=$(basename "$artifact_dir" | sed -n 's/size_artifacts_\(.*\)_gold/\1/p')
  CONFIG=gold

  if [[ -z "$PLATFORM" ]]; then
    echo "Could not extract platform from $artifact_dir"
    exit 1
  fi

  libcobalt_so_src="${artifact_dir}/libcobalt.so"

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
  RUNNER_PATH="${artifact_dir}/bin/run_cobalt_sizes"

  echo "Running Check binary size for $PLATFORM"
  vpython3 "$RUNNER_PATH" \
    --output-directory "$artifact_dir" \
    --isolated-script-test-output "$artifact_dir/sizes/test_results.json"

  SIZES_PATH="$artifact_dir/sizes/sizes/perf_results.json"

  echo "Running Compare binary size for $PLATFORM"
  python3 cobalt/testing/tools/compare_sizes.py \
    --current "$SIZES_PATH" \
    --reference cobalt/testing/tools/reference_metrics.json \
    --platform "${PLATFORM}" \
    --config "${CONFIG}"
else
  echo "Not a directory: $artifact_dir"
fi

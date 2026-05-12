#!/bin/bash
# Maintains summary results by copying files to summary and datadog directories.

run="$1"
passed="$2"
files_glob="$3"
summary_dir="$4"
datadog_dir="$5"
add_run_suffix_to_summary="$6"

shopt -s globstar nullglob
# Assumption: paths do not contain spaces.
for file_path in $files_glob; do
  if [[ -f "$file_path" ]]; then
    base=$(basename "$file_path")
    name="${base%.*}"
    ext="${base##*.}"

    # Always copy to datadog with run suffix
    cp "$file_path" "${datadog_dir}/${name}_run${run}.${ext}"

    # Copy to summary if it's run 1 or it passed
    if [[ $run -eq 1 ]] || [[ "${passed}" == "true" ]]; then
      if [[ "${add_run_suffix_to_summary}" == "true" ]]; then
        cp "$file_path" "${summary_dir}/${name}_run${run}.${ext}"
      else
        cp "$file_path" "${summary_dir}/${name}.${ext}"
      fi
    fi
  fi
done

#!/bin/bash
#
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Generates ads_hosts.csv by downloading ads.txt from the top 1000 websites
# (from CrUX) to find ad tech domains.

# Exit on error, undefined variable, or pipe failure
set -euo pipefail

# Global variable to be accessed by the trap handler
temp_dir=""

cleanup() {
  if [[ -n "${temp_dir}" && -d "${temp_dir}" ]]; then
    rm -rf "${temp_dir}"
  fi
}
trap cleanup EXIT

info() {
  echo "[INFO] $(date +'%Y-%m-%dT%H:%M:%S%z'): $*"
}

err() {
  echo "[ERROR] $(date +'%Y-%m-%dT%H:%M:%S%z'): $*" >&2
}

usage() {
  echo "Usage: $0 OUTPUT_CSV" >&2
  echo "  OUTPUT_CSV: Path to save the generated ads_hosts.csv file." >&2
}

main() {
  if [[ $# -ne 1 ]]; then
    err "Illegal number of parameters."
    usage
    exit 2
  fi

  local output_csv
  output_csv=$(realpath -m "$1")

  # Check for necessary commands
  local cmds=("curl" "wget" "gunzip" "awk" "sed" "sort" "uniq" "parallel"
              "grep" "realpath" "mktemp")
  for cmd in "${cmds[@]}"; do
    command -v "${cmd}" >/dev/null 2>&1 || \
      { err "${cmd} not found. Please install it."; exit 1; }
  done

  # Specific check for GNU parallel
  if ! parallel --version 2>/dev/null | grep -iq "gnu parallel"; then
    err "GNU parallel is required, but a different version was found."
    exit 1
  fi

  # Create a temporary directory for downloading the CrUX list
  temp_dir=$(mktemp -d)

  info "Downloading top 1000 sites list from CrUX..."
  curl -sL "https://github.com/zakird/crux-top-lists/raw/refs/heads/main/data/global/current.csv.gz" | \
    gunzip -c | \
    awk -F ',' 'NR > 1 && $2 + 0 <= 1000 { print $1 }' \
      > "${temp_dir}/top1000.txt"

  info "Downloading ads.txt to find ad tech domains (this may take a while)..."

  # Temporarily disable exit on error and pipefail for the download pipeline
  # since some sites don't have ads.txt files.
  set +e
  set +o pipefail

  # 1. Download ads.txt from the given URL.
  # 2. Filter out lines that don't contain a domain.
  # 3. Convert to lowercase.
  # 4. Remove the "ownerdomain" line.
  # 5. Sort and deduplicate the domains.
  fetch_ads_data() {
      local url="$1"
      # Clean the URL (remove trailing slash)
      local target=$(echo "$url" | sed 's/\/$//')

      wget -qO- "$target/ads.txt" 2>/dev/null | \
      grep -E "^[a-zA-Z0-9-]+\." | \
      awk -F, '{print tolower($1)}' | \
      grep -v "^ownerdomain$" | \
      grep -E "^[a-z0-9.-]+$" | \
      sort -u
  }

  # Export the function for GNU Parallel
  export -f fetch_ads_data

  # 1. Read the top 1000 sites.
  # 2. Run fetch_ads_data in parallel for each site.
  # 3. Count the number of times each domain appears.
  # 4. Filter out domains that appear in fewer than 5 ads.txt files.
  # 5. Output the domains to the target output file.
  cat "${temp_dir}/top1000.txt" | \
    parallel --timeout 3 --progress -j 32 fetch_ads_data {} | \
    sort | \
    uniq -c | \
    sort -nr | \
    awk '$1 >= 5 {print $1 "," $2}' > "${output_csv}"

  local download_ec=$?

  # Restore shell options
  set -e
  set -o pipefail

  if [[ ${download_ec} -ne 0 ]]; then
    err "The ads.txt download pipeline finished with a non-zero exit"
    err "code: ${download_ec}."
    exit 1
  else
    info "ads.txt download pipeline completed without error."
  fi

  if [[ ! -s "${output_csv}" ]]; then
    err "${output_csv} is missing or empty."
    exit 1
  else
    info "Generated ${output_csv} with $(wc -l < "${output_csv}") lines."
  fi
}

main "$@"

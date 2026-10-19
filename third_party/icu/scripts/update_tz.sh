#!/bin/bash
# Copyright (c) 2026 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Download the 4 files below from the ICU trunk and put them in
# source/data/misc to update the IANA timezone database.
#
#   metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
#
# For IANA Time zone database, see https://www.iana.org/time-zones

tmp_dir=~/tmp/icu-tz
repo_url="https://github.com/unicode-org/icu.git"
treeroot="$(dirname "$0")/.."
datapath="source/data/misc"

echo "[*] Performing sparse clone from the upstream repository to tmp directory"
rm -rf "$tmp_dir"
mkdir -p "$tmp_dir"

# Clone the repo with depth 1 and block downloading any files (--filter=blob:none)
git clone --filter=blob:none --no-checkout --depth 1 "$repo_url" "$tmp_dir"
cd "$tmp_dir" || exit 1

git sparse-checkout set --no-cone \
  "icu4c/${datapath}/metaZones.txt" \
  "icu4c/${datapath}/timezoneTypes.txt" \
  "icu4c/${datapath}/windowsZones.txt" \
  "icu4c/${datapath}/zoneinfo64.txt"

git checkout main

echo "[*] Copying timezone files to ICU tree root"
mkdir -p "${treeroot}/${datapath}"

for file in metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt; do
  cp "icu4c/${datapath}/${file}" "${treeroot}/${datapath}/${file}"
done

echo "[*] Cleaning up tmp directory"
cd - > /dev/null || exit 1
rm -rf "$tmp_dir"

read -p "Do you want to patch timezoneTypes.txt to remove 'Factory{\"Etc/Unknown\"}'? (See https://crbug.com/381620359) [y/N]: " response
if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
  echo "[*] Patching timezoneTypes.txt..."
  sed -i '/Factory{"Etc\/Unknown"}/d' "${treeroot}/${datapath}/timezoneTypes.txt"
fi

#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Download the 4 files below from the ICU trunk and put them in
# source/data/misc to update the IANA timezone database.
#
#   metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
#
# For IANA Time zone database, see https://www.iana.org/time-zones

# See
# https://github.blog/news-insights/product-news/sunsetting-subversion-support/
# Github no longer supports SVN. To download files, this script will download
# the repo as a tarball and extract from a temporary folder location

tmp_dir=~/tmp/icu-tz
repo_url="https://github.com/unicode-org/icu/archive/refs/heads/main.tar.gz"
tarball="${tmp_dir}/source.tar.gz"
treeroot="$(dirname "$0")/.."
datapath="source/data/misc"

# Check if the repo for $version is available.
if ! wget --spider $repo_url 2>/dev/null; then
  echo "$repo_url does not exists"
  exit 1
fi

echo "Download main from the upstream repository to tmp directory"
rm -rf $tmp_dir
mkdir -p $tmp_dir
curl -L $repo_url --output $tarball

echo "Extracting timezone files from main to ICU tree root"
for file in metaZones.txt timezoneTypes.txt windowsZones.txt zoneinfo64.txt
do
  tar -xf $tarball -C $treeroot "icu-main/icu4c/${datapath}/${file}" --strip-components=2
done

echo "Cleaning up tmp directory"
rm -rf $tmp_dir

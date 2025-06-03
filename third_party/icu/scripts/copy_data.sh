#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# # Use of this source code is governed by a BSD-style license that can be
# # found in the LICENSE file.
#
# This script is tested ONLY on Linux. It may not work correctly on
# Mac OS X.
#
set -e # exit if fail

if [ $# -lt 1 ];
then
  echo "Usage: "$0" (android|cast|chromeos|common|flutter|flutter_desktop|ios)" >&2
  exit 1
fi

TOPSRC="$(dirname "$0")/.."
source "${TOPSRC}/scripts/data_common.sh"


function copy_common {
  DATA_PREFIX="data/out/tmp/icudt${VERSION}"
  TZRES_PREFIX="data/out/build/icudt${VERSION}l"

  echo "Generating the big endian data bundle"
  LD_LIBRARY_PATH=lib bin/icupkg -tb --ignore-deps "${DATA_PREFIX}l.dat" "${DATA_PREFIX}b.dat"

  echo "Copying icudtl.dat and icudtlb.dat"
  for endian in l b
  do
    rm "${TOPSRC}/common/icudt${endian}.dat"
    cp "${DATA_PREFIX}${endian}.dat" "${TOPSRC}/common/icudt${endian}.dat"
  done

  echo "Copying metaZones.res, timezoneTypes.res, zoneinfo64.res"
  for tzfile in metaZones timezoneTypes zoneinfo64
  do
    rm "${TOPSRC}/tzres/${tzfile}.res"
    cp "${TZRES_PREFIX}/${tzfile}.res" "${TOPSRC}/tzres/${tzfile}.res"
  done

  echo "Done with copying pre-built ICU data files."
}

function copy_data {
  echo "Copying icudtl.dat for $1"

  rm -f "${TOPSRC}/$2/icudtl.dat"
  cp "data/out/tmp/icudt${VERSION}l.dat" "${TOPSRC}/$2/icudtl.dat"

  echo "Done with copying pre-built ICU data file for $1."
}

function copy_hash_data {
  echo "Copying icudtl.dat.hash for $1"

  rm -f "${TOPSRC}/$2/icudtl.dat.hash"
  cp "data/out/tmp/icudt${VERSION}l.dat.hash" "${TOPSRC}/$2/icudtl.dat.hash"

  echo "Done with copying icudtl.dat.hash for $1."
}

function align_data {
  echo "Aligning files in icudtl.dat for $1"

  local ORIGINAL="data/out/tmp/icudt${VERSION}l.dat"
  local ALIGNED="data/out/tmp/icudt${VERSION}l-aligned.dat"

  rm -f "${ALIGNED}"
  "${TOPSRC}/scripts/icualign.py" "${ORIGINAL}" "${ALIGNED}"
  mv "${ALIGNED}" "${ORIGINAL}"

  echo "Done with aligning files in icudtl.dat for $1."
}

function hash_data {
  echo "Hashing icudtl.dat for $1"

  local DATA_FILE="data/out/tmp/icudt${VERSION}l.dat"
  local HASH_FILE="data/out/tmp/icudt${VERSION}l.dat.hash"

  rm -f "${HASH_FILE}"
  "$TOPSRC/scripts/icuhash.py" "${DATA_FILE}" "${HASH_FILE}"

  echo "Done with hashing icudtl.dat for $1."
}


BACKUP_DIR="dataout/$1"
function backup_outdir {
  rm -rf "${BACKUP_DIR}"
  mkdir -p "${BACKUP_DIR}"
  find "data/out" | cpio -pdmv "${BACKUP_DIR}"
}

case "$1" in
  "chromeos")
    align_data ChromeOS
    hash_data ChromeOS
    copy_data ChromeOS $1
    copy_hash_data ChromeOS $1
    backup_outdir $1
    ;;
  "common")
    copy_common
    backup_outdir $1
    ;;
  "android")
    copy_data Android $1
    backup_outdir $1
    ;;
  "ios")
    copy_data iOS $1
    backup_outdir $1
    ;;
  "cast")
    copy_data Cast $1
    backup_outdir $1
    ;;
  "flutter")
    copy_data Flutter $1
    backup_outdir $1
    ;;
  "flutter_desktop")
    copy_data Flutter_Desktop $1
    backup_outdir $1
    ;;
esac

#!/bin/bash

set -x -e # stop if fail

ICUROOT="$(dirname "$0")/.."

function config_data {
  if [ $# -lt 1 ];
  then
    echo "config target missing." >&2
    echo "Should be (android|cast|chromeos|common|flutter|flutter_desktop|ios)" >&2
    exit 1
  fi

  ICU_DATA_FILTER_FILE="${ICUROOT}/filters/$1.json" \
  "${ICUROOT}/source/runConfigureICU" --enable-debug --disable-release \
    Linux/gcc --disable-tests  --disable-layoutex --enable-rpath \
    --prefix="$(pwd)" || \
    { echo "failed to configure data for $1" >&2; exit 1; }
}

echo "Build the necessary tools"
"${ICUROOT}/source/runConfigureICU" --enable-debug --disable-release \
    Linux/gcc  --disable-tests --disable-layoutex --enable-rpath \
    --prefix="$(pwd)"
make -j 120

echo "Build the filtered data for common"
(cd data && make clean)
config_data common
make -j 120
$ICUROOT/scripts/copy_data.sh common

echo "Build the filtered data for chromeos"
(cd data && make clean)
config_data chromeos
make -j 120
$ICUROOT/scripts/copy_data.sh chromeos

echo "Build the filtered data for Cast"
(cd data && make clean)
config_data cast
$ICUROOT/cast/patch_locale.sh || exit 1
make -j 120
$ICUROOT/scripts/copy_data.sh cast

echo "Build the filtered data for Android"
(cd data && make clean)
config_data android
make -j 120
$ICUROOT/scripts/copy_data.sh android

echo "Build the filtered data for iOS"
(cd data && make clean)
config_data ios
make -j 120
$ICUROOT/scripts/copy_data.sh ios

echo "Build the filtered data for Flutter"
(cd data && make clean)
config_data flutter
$ICUROOT/flutter/patch_brkitr.sh || exit 1
make -j 120
$ICUROOT/scripts/copy_data.sh flutter

echo "Build the filtered data for Flutter Desktop"
${ICUROOT}/scripts/clean_up_data_source.sh
(cd data && make clean)
config_data flutter_desktop
make -j 120
$ICUROOT/scripts/copy_data.sh flutter_desktop

echo "Clean up the git"
$ICUROOT/scripts/clean_up_data_source.sh

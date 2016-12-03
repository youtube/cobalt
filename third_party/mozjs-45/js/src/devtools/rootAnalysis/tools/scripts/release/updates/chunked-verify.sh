#!/bin/sh
set -e
set -x
# This ugly hack is a cross-platform (Linux/Mac/Windows+MSYS) way to get the
# absolute path to the directory containing this script
pushd `dirname $0` &>/dev/null
MY_DIR=$(pwd)
popd &>/dev/null
SCRIPTS_DIR="$MY_DIR/../../../"
PYTHON="/tools/python/bin/python"
if [ ! -x $PYTHON ]; then
    PYTHON=python
fi
JSONTOOL="$PYTHON $SCRIPTS_DIR/buildfarm/utils/jsontool.py"
workdir=`pwd`

platform=$1
configDict=$2
chunks=$3
thisChunk=$4
channel=$5
releaseConfig=$($JSONTOOL -k properties.release_config $PROPERTIES_FILE)
releaseTag=$($JSONTOOL -k properties.release_tag $PROPERTIES_FILE)
slavebuilddir=$($JSONTOOL -k properties.slavebuilddir $PROPERTIES_FILE)

if [ -z "$BUILDBOT_CONFIGS" ]; then
    export BUILDBOT_CONFIGS="https://hg.mozilla.org/build/buildbot-configs"
fi

$PYTHON -u $SCRIPTS_DIR/buildfarm/maintenance/purge_builds.py \
-s 16 -n info -n 'rel-*' -n 'tb-rel-*' -n $slavebuilddir

$PYTHON $MY_DIR/chunked-verify.py -t $releaseTag -r $releaseConfig \
  -b $BUILDBOT_CONFIGS -p $platform --chunks $chunks --this-chunk $thisChunk \
  --config-dict $2 --release-channel $channel

#!/bin/sh
set -e
set -x
# This ugly hack is a cross-platform (Linux/Mac/Windows+MSYS) way to get the
# absolute path to the directory containing this script
pushd `dirname $0` &>/dev/null
MY_DIR=$(pwd)
popd &>/dev/null
SCRIPTS_DIR="$MY_DIR/../../"
PYTHON="/tools/python/bin/python"
if [ ! -x $PYTHON ]; then
    PYTHON=python
fi
JSONTOOL="$PYTHON $SCRIPTS_DIR/buildfarm/utils/jsontool.py"
workdir=`pwd`

branchConfig=$1
shift

branch=$(basename $($JSONTOOL -k properties.branch $PROPERTIES_FILE))
builder=$($JSONTOOL -k properties.buildername $PROPERTIES_FILE)
slavebuilddir=$($JSONTOOL -k properties.slavebuilddir $PROPERTIES_FILE)
slavename=$($JSONTOOL -k properties.slavename $PROPERTIES_FILE)
master=$($JSONTOOL -k properties.master $PROPERTIES_FILE)
releaseConfig=$($JSONTOOL -k properties.release_config $PROPERTIES_FILE)
releaseTag=$($JSONTOOL -k properties.script_repo_revision $PROPERTIES_FILE)

if [ -z "$BUILDBOT_CONFIGS" ]; then
    export BUILDBOT_CONFIGS="https://hg.mozilla.org/build/buildbot-configs"
fi
if [ -z "$CLOBBERER_URL" ]; then
    export CLOBBERER_URL="https://api.pub.build.mozilla.org/clobberer/lastclobber"
fi

export MOZ_SIGN_CMD="$MOZ_SIGN_CMD"

cd $SCRIPTS_DIR/../..
$PYTHON $SCRIPTS_DIR/clobberer/clobberer.py -s scripts -s buildprops.json \
  -s token -s nonce \
  $CLOBBERER_URL $branch $builder $slavebuilddir $slavename $master
cd $SCRIPTS_DIR/..
$PYTHON -u $SCRIPTS_DIR/buildfarm/maintenance/purge_builds.py \
  -s 0.5 -n info -n 'rel-*' -n 'tb-rel-*' -n $slavebuilddir
cd $workdir

$PYTHON $MY_DIR/generate-sums.py -c $branchConfig -r $releaseConfig \
  -b $BUILDBOT_CONFIGS -t $releaseTag $@

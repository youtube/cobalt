#!/bin/sh
set -e
set -x
MY_DIR=$(dirname $(readlink -f $0))
SCRIPTS_DIR="$MY_DIR/../../"
if [ -x "/tools/python/bin/python" ]; then
    PYTHON="/tools/python/bin/python"
else
    PYTHON=`which python`
fi

JSONTOOL="$PYTHON $SCRIPTS_DIR/buildfarm/utils/jsontool.py"
workdir=`pwd`

releaseConfig=$($JSONTOOL -k properties.release_config $PROPERTIES_FILE)
releaseTag=$($JSONTOOL -k properties.script_repo_revision $PROPERTIES_FILE)
# Clobberer requires the short name of the branch
branch=$(basename $($JSONTOOL -k properties.branch $PROPERTIES_FILE))
builder=$($JSONTOOL -k properties.buildername $PROPERTIES_FILE)
slavebuilddir=$($JSONTOOL -k properties.slavebuilddir $PROPERTIES_FILE)
slavename=$($JSONTOOL -k properties.slavename $PROPERTIES_FILE)
master=$($JSONTOOL -k properties.master $PROPERTIES_FILE)
if [ -n "$EXTRA_DATA" ]; then
  tag_extra_args=$($JSONTOOL -k tag_args $EXTRA_DATA)
  clobber_extra_args="-s $EXTRA_DATA"
fi
if [ -z "$BUILDBOT_CONFIGS" ]; then
    export BUILDBOT_CONFIGS="https://hg.mozilla.org/build/buildbot-configs"
fi
if [ -z "$CLOBBERER_URL" ]; then
    export CLOBBERER_URL="https://api.pub.build.mozilla.org/clobberer/lastclobber"
fi

cd $SCRIPTS_DIR/../..
echo "Calling clobberer: $PYTHON $SCRIPTS_DIR/clobberer/clobberer.py -s scripts -s buildprops.json $clobber_extra_args $CLOBBERER_URL $branch $builder $slavebuilddir $slavename $master"
$PYTHON $SCRIPTS_DIR/clobberer/clobberer.py -s scripts -s buildprops.json $clobber_extra_args $CLOBBERER_URL $branch $builder $slavebuilddir $slavename $master
cd $SCRIPTS_DIR/..
$PYTHON -u $SCRIPTS_DIR/buildfarm/maintenance/purge_builds.py \
  -s 3 -n info -n 'rel-*' -n 'tb-rel-*' -n $slavebuilddir
cd $workdir

echo "Calling tag-release.py: $PYTHON tag-release.py -c $releaseConfig -b $BUILDBOT_CONFIGS -t $releaseTag $tag_extra_args"
$PYTHON $MY_DIR/tag-release.py -c $releaseConfig -b $BUILDBOT_CONFIGS -t $releaseTag $tag_extra_args || exit 2

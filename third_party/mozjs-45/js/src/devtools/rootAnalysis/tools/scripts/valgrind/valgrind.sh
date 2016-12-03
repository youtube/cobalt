#!/bin/bash
set -e
set -x
SCRIPTS_DIR="$(readlink -f $(dirname $0)/../..)"

if [ -f "$PROPERTIES_FILE" ]; then
    PYTHON="/tools/python/bin/python"
    [ -x $PYTHON ] || PYTHON="${PYTHON}2.7"
    [ -x $PYTHON ] || PYTHON=$(which python2.7)
    [ -x $PYTHON ] || PYTHON=python
    JSONTOOL="$PYTHON $SCRIPTS_DIR/buildfarm/utils/jsontool.py"

    builder=$($JSONTOOL -k properties.buildername $PROPERTIES_FILE)
    slavename=$($JSONTOOL -k properties.slavename $PROPERTIES_FILE)
    master=$($JSONTOOL -k properties.master $PROPERTIES_FILE)
    branch=$($JSONTOOL -k properties.branch $PROPERTIES_FILE)
    REVISION=$($JSONTOOL -k properties.revision $PROPERTIES_FILE)

    BRANCHES_JSON=$SCRIPTS_DIR/buildfarm/maintenance/production-branches.json

    HG_REPO=$($JSONTOOL -k ${branch}.repo $BRANCHES_JSON)
    HG_BUNDLE="http://ftp.mozilla.org/pub/mozilla.org/firefox/bundles/${branch}.hg"

    builddir=$(basename $(readlink -f .))

    # Clobbering
    if [ -z "$CLOBBERER_URL" ]; then
        export CLOBBERER_URL="https://api.pub.build.mozilla.org/clobberer/lastclobber"
    fi

    (cd $SCRIPTS_DIR/../..
    $PYTHON $SCRIPTS_DIR/clobberer/clobberer.py -s scripts -s $(basename $PROPERTIES_FILE) \
        $CLOBBERER_URL $branch "$builder" $builddir $slavename $master)

    # Purging
    (cd $SCRIPTS_DIR/..
    $PYTHON -u $SCRIPTS_DIR/buildfarm/maintenance/purge_builds.py \
        -s 8 -n info -n 'rel-*' -n 'tb-rel-*' -n $builddir)
fi
if [ -z "$HG_REPO" ]; then
    export HG_REPO="https://hg.mozilla.org/mozilla-central"
    export HG_BUNDLE="https://ftp.mozilla.org/pub/mozilla.org/firefox/bundles/mozilla-central.hg"
fi
if [ -z "$REVISION" ]; then
    export REVISION="default"
fi

$PYTHON $SCRIPTS_DIR/buildfarm/utils/retry.py -s 1 -r 5 -t 3660 \
     $PYTHON $SCRIPTS_DIR/buildfarm/utils/hgtool.py --rev $REVISION \
          --bundle $HG_BUNDLE $HG_REPO src || exit 2

# Put our short revisions into the properties directory for consumption by buildbot.
if [ ! -d properties ]; then
    mkdir properties
fi
pushd src; GOT_REVISION=`hg parent --template={node} | cut -c1-12`; popd
echo "revision: $GOT_REVISION" > properties/revision
echo "got_revision: $GOT_REVISION" > properties/got_revision

if [ -f src/build/valgrind/valgrind.sh ]; then
    bash src/build/valgrind/valgrind.sh
    exit $?
fi

srcdir=$PWD/src
objdir=${MOZ_OBJDIR-objdir}

# If the objdir is a relative path, it is relative to the srcdir.
case "$objdir" in
    /*)
	;;
    *)
        objdir="$srcdir/$objdir"
	;;
esac

if [ ! -d $objdir ]; then
    mkdir $objdir
fi
cd $objdir

if [ "`uname -m`" = "x86_64" ]; then
    export LD_LIBRARY_PATH=/tools/gcc-4.5-0moz3/installed/lib64
    _arch=64
else
    export LD_LIBRARY_PATH=/tools/gcc-4.5-0moz3/installed/lib
    _arch=32
fi

# Note: an exit code of 2 turns the job red on TBPL.
MOZCONFIG=$srcdir/browser/config/mozconfigs/linux${_arch}/valgrind make -f $srcdir/client.mk configure || exit 2
make -j4 || exit 2
make package || exit 2

# We need to set MOZBUILD_STATE_PATH so that |mach| skips its first-run
# initialization step and actually runs the |valgrind-test| command.
export MOZBUILD_STATE_PATH=.

# |mach valgrind-test|'s exit code will be 1 (which turns the job orange on
# TBPL) if Valgrind finds errors, and 2 (which turns the job red) if something
# else goes wrong, such as Valgrind crashing.
python2.7 $srcdir/mach valgrind-test
exit $?

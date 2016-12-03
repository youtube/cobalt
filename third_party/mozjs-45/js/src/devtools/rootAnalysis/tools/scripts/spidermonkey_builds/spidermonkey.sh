#!/bin/bash
set -e
set -x
pushd $(dirname $0)/../../ > /dev/null
SCRIPTS_DIR=$PWD
popd > /dev/null

SPIDERDIR=$SCRIPTS_DIR/scripts/spidermonkey_builds

DEFAULT_REPO="https://hg.mozilla.org/integration/mozilla-inbound"

function usage() {
  echo "Usage: $0 [-m mirror_url] [-b bundle_url] [-r revision] [--dep] variant"
  if [ -z "$PROPERTIES_FILE" ]; then
    echo "PROPERTIES_FILE must be set for an automation build"
  fi
}

# It doesn't work to just pull from try. If you try to pull the full repo,
# it'll error out because it's too big. Even if you restrict to a particular
# revision, the pull is painfully slow (as in, it could take days) without
# --bundle and/or --mirror.
hgtool_args=()
noclean=""
VARIANT=""
while [ $# -gt 0 ]; do
    case "$1" in
        -m|--mirror)
            shift
            hgtool_args+=(--mirror "$1")
            shift
            ;;
        -b|--bundle)
            shift
            hgtool_args+=(--bundle "$1")
            shift
            ;;
        --ttserver)
            # Note that this script (and tooltool_wrapper.sh, and tooltool.py)
            # only accepts a single tooltool server, so all but the last will
            # be ignored.
            shift
            TT_SERVER="$1"
            shift
            ;;
        -r|--rev)
            shift
            hgtool_args+=(--clone-by-revision -r "$1")
            shift
            ;;
        --dep)
            shift
            noclean=1
            ;;
        --platform)
            shift
            platform="$1"
            shift
            ;;
        *)
            if [ -z "$VARIANT" ]; then
                VARIANT="$1"
                shift
            else
                echo "Invalid arguments: multiple variants detected" >&2
                usage
                exit 1
            fi
            ;;
    esac
done

if [ -z "$VARIANT" ]; then
    usage
    exit 1
fi

if [ -n "$PROPERTIES_FILE" ]; then
    if ! [ -f "$PROPERTIES_FILE" ]; then
        echo "Properties file '$PROPERTIES_FILE' not found, aborting" >&2
        exit 1
    fi
    echo "Properties file found; running automation build"
    export AUTOMATION=1
    HG_REPO=${HG_REPO:-$DEFAULT_REPO}
else
    echo "Properties file not set; running development build"
    unset AUTOMATION
fi

PYTHON="python"
if [ -f "$PROPERTIES_FILE" ]; then
    JSONTOOL="$PYTHON $SCRIPTS_DIR/buildfarm/utils/jsontool.py"

    builder=$($JSONTOOL -k properties.buildername $PROPERTIES_FILE)
    slavename=$($JSONTOOL -k properties.slavename $PROPERTIES_FILE)
    master=$($JSONTOOL -k properties.master $PROPERTIES_FILE)

    builddir=$(basename $PWD)
    branch=$(basename $HG_REPO)

    # Clobbering
    if [ -z "$CLOBBERER_URL" ]; then
        export CLOBBERER_URL="https://api.pub.build.mozilla.org/clobberer/lastclobber"
    fi

    cd $SCRIPTS_DIR/../..
    $PYTHON $SCRIPTS_DIR/clobberer/clobberer.py -s scripts \
        -s ${PROPERTIES_FILE##*/} \
        $CLOBBERER_URL $branch "$builder" $builddir $slavename $master || true

    # Purging
    cd $SCRIPTS_DIR/..
    $PYTHON -u $SCRIPTS_DIR/buildfarm/maintenance/purge_builds.py \
        -s 4 -n info -n 'rel-*' -n 'tb-rel-*' -n $builddir
fi

if [ -z "$HG_REPO" ] || [ "$HG_REPO" = none ]; then
  SOURCE=.
else
  $PYTHON $SCRIPTS_DIR/buildfarm/utils/hgtool.py "${hgtool_args[@]}" $HG_REPO src || exit 2
  SOURCE=src

  # Pull down some standard tools that the build seems to have started
  # requiring, eg mozmake on windows.
  if [ "$OSTYPE" = "msys" ] && [ -n "$platform" ]; then
      if [ -z "$TT_SERVER" ]; then
          echo "Error: tooltool base url not set (use --ttserver command line option or TT_SERVER environment variable)" >&2
          exit 1
      fi
      $SCRIPTS_DIR/scripts/tooltool/tooltool_wrapper.sh $SOURCE/browser/config/tooltool-manifests/${platform%-debug}/releng.manifest $TT_SERVER setup.sh 'c:\mozilla-build\python27\python.exe' C:/mozilla-build/tooltool.py
  fi
fi

# The build script has been moved into the tree, but this script needs to keep
# working for older branches.
if [ -x "$SOURCE/js/src/devtools/automation/autospider.sh" ]; then
    ARGS=""
    if [ -n "$noclean" ]; then
        ARGS="$ARGS --dep"
    fi
    if [ -n "$platform" ]; then
        ARGS="$ARGV --platform $platform"
    fi
    exec $SOURCE/js/src/devtools/automation/autospider.sh $ARGS "$VARIANT"
    exit 1
fi

# Everything from here down should be deleted when the oldest branch contains
# the autospider.sh script.

if [ ! -f "$SPIDERDIR/$VARIANT" ]; then
    echo "Could not find $VARIANT"
    usage
    exit 1
fi

(cd $SOURCE/js/src; autoconf-2.13 || autoconf2.13)

TRY_OVERRIDE=$SOURCE/js/src/config.try
if [ -r $TRY_OVERRIDE ]; then
  CONFIGURE_ARGS=$(cat $TRY_OVERRIDE)
else
  CONFIGURE_ARGS=$(cat $SPIDERDIR/$VARIANT)
fi

# Always do clobber builds. They're fast.
if [ -z "$noclean" ]; then
  [ -d objdir ] && rm -rf objdir
  mkdir objdir
else
  [ -d objdir ] || mkdir objdir
fi
cd objdir

OBJDIR=$PWD

echo OBJDIR is $OBJDIR

USE_64BIT=false

if [[ "$OSTYPE" == darwin* ]]; then
  USE_64BIT=true
elif [ "$OSTYPE" = "linux-gnu" ]; then
  if [ -n "$AUTOMATION" ]; then
      GCCDIR="${GCCDIR:-/tools/gcc-4.7.2-0moz1}"
      CONFIGURE_ARGS="$CONFIGURE_ARGS --with-ccache"
  fi
  UNAME_M=$(uname -m)
  MAKEFLAGS=-j4
  if [ "$VARIANT" = "arm-sim" ]; then
    USE_64BIT=false
  elif [ "$UNAME_M" = "x86_64" ]; then
    USE_64BIT=true
  fi

  if [ "$UNAME_M" != "arm" ] && [ -n "$AUTOMATION" ]; then
    export CC=$GCCDIR/bin/gcc
    export CXX=$GCCDIR/bin/g++
    if $USE_64BIT; then
      export LD_LIBRARY_PATH=$GCCDIR/lib64
    else
      export LD_LIBRARY_PATH=$GCCDIR/lib
    fi
  fi
elif [ "$OSTYPE" = "msys" ]; then
  USE_64BIT=false
  MAKE=${MAKE:-mozmake}
fi

MAKE=${MAKE:-make}

if $USE_64BIT; then
  NSPR64="--enable-64bit"
else
  NSPR64=""
  if [ "$OSTYPE" != "msys" ]; then
    export CC="${CC:-/usr/bin/gcc} -m32"
    export CXX="${CXX:-/usr/bin/g++} -m32"
    export AR=ar
  fi
fi

../$SOURCE/js/src/configure $CONFIGURE_ARGS --enable-nspr-build --prefix=$OBJDIR/dist || exit 2
$MAKE -s -w -j4 || exit 2
cp -p ../$SOURCE/build/unix/run-mozilla.sh $OBJDIR/dist/bin

# The Root Analysis tests run in a special GC Zeal mode and disable ASLR to
# make tests reproducible.
COMMAND_PREFIX=''
if [[ "$VARIANT" = "rootanalysis" ]]; then
    export JS_GC_ZEAL=7

    # rootanalysis builds are currently only done on Linux, which should have
    # setarch, but just in case we enable them on another platform:
    if type setarch >/dev/null 2>&1; then
        COMMAND_PREFIX="setarch $(uname -m) -R "
    fi
elif [[ "$VARIANT" = "generational" ]]; then
    # Generational is currently being used for compacting GC
    export JS_GC_ZEAL=14

    # rootanalysis builds are currently only done on Linux, which should have
    # setarch, but just in case we enable them on another platform:
    if type setarch >/dev/null 2>&1; then
        COMMAND_PREFIX="setarch $(uname -m) -R "
    fi
fi

$COMMAND_PREFIX $MAKE check || exit 1
$COMMAND_PREFIX $MAKE check-jit-test || exit 1
$COMMAND_PREFIX $MAKE check-jstests || exit 1
$COMMAND_PREFIX $OBJDIR/dist/bin/jsapi-tests || exit 1

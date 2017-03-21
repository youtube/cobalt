#!/bin/bash
#
# setup_toolchain.sh: Sets toolchain environment variables used by other scripts.

# Fail-fast if anything in the script fails.
set -e

# check that the preconditions for this script are met
if [ $(type -t verbose) != 'function' ]; then
  echo "ERROR: The verbose function is expected to be defined"
  return 1
fi

if [ $(type -t exportVar) != 'function' ]; then
  echo "ERROR: The exportVar function is expected to be defined"
  return 1
fi

if [ $(type -t absPath) != 'function' ]; then
  echo "ERROR: The absPath function is expected to be defined"
  return 1
fi

if [ -z "$SCRIPT_DIR" ]; then
  echo "ERROR: The SCRIPT_DIR variable is expected to be defined"
  return 1
fi

function default_toolchain() {
  NDK_REV=${NDK_REV-10exp}
  ANDROID_ARCH=${ANDROID_ARCH-arm}
  
  if [[ $ANDROID_ARCH == *64* ]]; then
    API_LEVEL=L # Experimental Android L-Release system images
  else
    API_LEVEL=14 # Official Android 4.0 system images  
  fi

  TOOLCHAIN_DIR=${SCRIPT_DIR}/../toolchains
  if [ $(uname) == "Darwin" ]; then
    verbose "Using Mac toolchain."
    TOOLCHAIN_TYPE=ndk-r$NDK_REV-$ANDROID_ARCH-darwin_v$API_LEVEL
  else
    verbose "Using Linux toolchain."
    TOOLCHAIN_TYPE=ndk-r$NDK_REV-$ANDROID_ARCH-linux_v$API_LEVEL
  fi
  exportVar ANDROID_TOOLCHAIN "${TOOLCHAIN_DIR}/${TOOLCHAIN_TYPE}/bin"

  # if the toolchain doesn't exist on your machine then we need to fetch it
  if [ ! -d "$ANDROID_TOOLCHAIN" ]; then
    mkdir -p $TOOLCHAIN_DIR
    # enter the toolchain directory then download, unpack, and remove the tarball
    pushd $TOOLCHAIN_DIR
    TARBALL=ndk-r$NDK_REV-v$API_LEVEL.tgz

    ${SCRIPT_DIR}/download_toolchains.py \
        http://chromium-skia-gm.commondatastorage.googleapis.com/android-toolchains/$TARBALL \
        $TOOLCHAIN_DIR/$TARBALL
    tar -xzf $TARBALL $TOOLCHAIN_TYPE
    rm $TARBALL
    popd
  fi 

  verbose "Targeting NDK API $API_LEVEL for use on Android 4.0 (NDK Revision $NDK_REV) and above"  
}

#check to see if the toolchain has been defined and if not setup the default toolchain
if [ -z "$ANDROID_TOOLCHAIN" ]; then
  default_toolchain
  if [ ! -d "$ANDROID_TOOLCHAIN" ]; then
    echo "ERROR: unable to download/setup the required toolchain (${ANDROID_TOOLCHAIN})"
    return 1;
  fi
fi

GCC=$(command ls $ANDROID_TOOLCHAIN/*-gcc | head -n1)
if [ -z "$GCC" ]; then
  echo "ERROR: Could not find Android cross-compiler in: $ANDROID_TOOLCHAIN"
  return 1
fi

# Remove the '-gcc' at the end to get the full toolchain prefix
ANDROID_TOOLCHAIN_PREFIX=${GCC%%-gcc}

CCACHE=${ANDROID_MAKE_CCACHE-$(which ccache || true)}

exportVar CC "$CCACHE $ANDROID_TOOLCHAIN_PREFIX-gcc"
exportVar CXX "$CCACHE $ANDROID_TOOLCHAIN_PREFIX-g++"
exportVar LINK "$CCACHE $ANDROID_TOOLCHAIN_PREFIX-gcc"

exportVar AR "$ANDROID_TOOLCHAIN_PREFIX-ar"
exportVar RANLIB "$ANDROID_TOOLCHAIN_PREFIX-ranlib"
exportVar OBJCOPY "$ANDROID_TOOLCHAIN_PREFIX-objcopy"
exportVar STRIP "$ANDROID_TOOLCHAIN_PREFIX-strip"

# Create symlinks for nm & readelf and add them to the path so that the ninja
# build uses them instead of attempting to use the one on the system.
# This is required to build using ninja on a Mac.
ln -sf $ANDROID_TOOLCHAIN_PREFIX-nm $ANDROID_TOOLCHAIN/nm
ln -sf $ANDROID_TOOLCHAIN_PREFIX-readelf $ANDROID_TOOLCHAIN/readelf
ln -sf $ANDROID_TOOLCHAIN_PREFIX-as $ANDROID_TOOLCHAIN/as
exportVar PATH $ANDROID_TOOLCHAIN:$PATH

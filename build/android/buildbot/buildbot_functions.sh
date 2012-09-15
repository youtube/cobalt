#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Bash functions used by buildbot annotator scripts for the android
# build of chromium.  Executing this script should not perform actions
# other than setting variables and defining of functions.

# Number of jobs on the compile line; e.g.  make -j"${JOBS}"
JOBS="${JOBS:-4}"

# Clobber build?  Overridden by bots with BUILDBOT_CLOBBER.
NEED_CLOBBER="${NEED_CLOBBER:-0}"


# Parse named arguments passed into the annotator script
# and assign them global variable names.
function bb_parse_args {
  while [[ $1 ]]; do
    case "$1" in
      --factory-properties=*)
        FACTORY_PROPERTIES="$(echo "$1" | sed 's/^[^=]*=//')"
        BUILDTYPE=$(bb_get_json_prop "$FACTORY_PROPERTIES" target)
        ;;
      --build-properties=*)
        BUILD_PROPERTIES="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      *)
        echo "@@@STEP_WARNINGS@@@"
        echo "Warning, unparsed input argument: '$1'"
        ;;
    esac
    shift
  done
}

# Function to force-green a bot.
function bb_force_bot_green_and_exit {
  echo "@@@BUILD_STEP Bot forced green.@@@"
  exit 0
}

function bb_run_gclient_hooks {
  gclient runhooks
}

# Basic setup for all bots to run after a source tree checkout.
# Args:
#   $1: source root.
#   $2 and beyond: key value pairs which are parsed by bb_parse_args.
function bb_baseline_setup {
  echo "@@@BUILD_STEP Environment setup@@@"
  SRC_ROOT="$1"
  # Remove SRC_ROOT param
  shift

  bb_parse_args "$@"

  cd $SRC_ROOT
  if [ ! "$BUILDBOT_CLOBBER" = "" ]; then
    NEED_CLOBBER=1
  fi


  local BUILDTOOL=$(bb_get_json_prop "$FACTORY_PROPERTIES" buildtool)
  if [[ $BUILDTOOL = ninja ]]; then
    export GYP_GENERATORS=ninja
  fi

  if [ "$NEED_CLOBBER" -eq 1 ]; then
    echo "@@@BUILD_STEP Clobber@@@"
    # Sdk key expires, delete android folder.
    # crbug.com/145860
    rm -rf ~/.android
    rm -rf "${SRC_ROOT}"/out
    if [ -e "${SRC_ROOT}"/out ] ; then
      echo "Clobber appeared to fail?  ${SRC_ROOT}/out still exists."
      echo "@@@STEP_WARNINGS@@@"
    fi
  fi

  bb_setup_goma_internal
  . build/android/envsetup.sh
  export GYP_DEFINES+=" fastbuild=1"

  # Should be called only after envsetup is done.
  bb_run_gclient_hooks
}


# Setup goma.  Used internally to buildbot_functions.sh.
function bb_setup_goma_internal {
  export GOMA_DIR=/b/build/goma
  export GOMA_API_KEY_FILE=${GOMA_DIR}/goma.key
  export GOMA_COMPILER_PROXY_DAEMON_MODE=true
  export GOMA_COMPILER_PROXY_RPC_TIMEOUT_SECS=300

  echo "Killing old goma processes"
  ${GOMA_DIR}/goma_ctl.sh stop || true
  killall compiler_proxy || true

  echo "Starting goma"
  ${GOMA_DIR}/goma_ctl.sh start
  trap bb_stop_goma_internal SIGHUP SIGINT SIGTERM
}

# Stop goma.
function bb_stop_goma_internal {
  echo "Stopping goma"
  ${GOMA_DIR}/goma_ctl.sh stop
}

# $@: make args.
# Use goma if possible; degrades to non-Goma if needed.
function bb_goma_make {
  if [ "${GOMA_DIR}" = "" ]; then
    make -j${JOBS} "$@"
    return
  fi

  HOST_CC=$GOMA_DIR/gcc
  HOST_CXX=$GOMA_DIR/g++
  TARGET_CC=$(/bin/ls $ANDROID_TOOLCHAIN/*-gcc | head -n1)
  TARGET_CXX=$(/bin/ls $ANDROID_TOOLCHAIN/*-g++ | head -n1)
  TARGET_CC="$GOMA_DIR/gomacc $TARGET_CC"
  TARGET_CXX="$GOMA_DIR/gomacc $TARGET_CXX"
  COMMON_JAVAC="$GOMA_DIR/gomacc /usr/bin/javac -J-Xmx512M \
    -target 1.5 -Xmaxerrs 9999999"

  command make \
    -j100 \
    -l20 \
    HOST_CC="$HOST_CC" \
    HOST_CXX="$HOST_CXX" \
    TARGET_CC="$TARGET_CC" \
    TARGET_CXX="$TARGET_CXX" \
    CC.host="$HOST_CC" \
    CXX.host="$HOST_CXX" \
    CC.target="$TARGET_CC" \
    CXX.target="$TARGET_CXX" \
    LINK.target="$TARGET_CXX" \
    COMMON_JAVAC="$COMMON_JAVAC" \
    BUILDTYPE="$BUILDTYPE" \
    "$@"

  local make_exit_code=$?
  return $make_exit_code
}

# Build using ninja.
function bb_goma_ninja {
  echo "Using ninja to build."
  local TARGET=$1
  ninja -C out/$BUILDTYPE -j120 -l20 $TARGET
}

# Compile step
function bb_compile {
  # This must be named 'compile', not 'Compile', for CQ interaction.
  # Talk to maruel for details.
  echo "@@@BUILD_STEP compile@@@"

  BUILDTOOL=$(bb_get_json_prop "$FACTORY_PROPERTIES" buildtool)
  if [[ $BUILDTOOL = ninja ]]; then
    bb_goma_ninja All
  else
    bb_goma_make
  fi

  bb_stop_goma_internal
}

# Experimental compile step; does not turn the tree red if it fails.
function bb_compile_experimental {
  # Linking DumpRenderTree appears to hang forever?
  EXPERIMENTAL_TARGETS="android_experimental"
  for target in ${EXPERIMENTAL_TARGETS} ; do
    echo "@@@BUILD_STEP Experimental Compile $target @@@"
    set +e
    if [[ $BUILDTOOL = ninja ]]; then
      bb_goma_ninja "${target}"
    else
      bb_goma_make -k "${target}"
    fi
    if [ $? -ne 0 ] ; then
      echo "@@@STEP_WARNINGS@@@"
    fi
    set -e
  done
}

# Run tests on an emulator.
function bb_run_tests_emulator {
  echo "@@@BUILD_STEP Run Tests on an Emulator@@@"
  build/android/run_tests.py -e --xvfb --verbose
}

# Run tests on an actual device.  (Better have one plugged in!)
function bb_run_unit_tests {
  python build/android/device_status_check.py
  local LOGCAT_DUMP_DIR="$CHROME_SRC/out/logcat"
  rm -rf "$LOGCAT_DUMP_DIR"
  python build/android/adb_logcat_monitor.py "$LOGCAT_DUMP_DIR" &

  build/android/run_tests.py --xvfb --verbose

  echo "@@@BUILD_STEP Logcat dump@@@"
  python build/android/adb_logcat_printer.py "$LOGCAT_DUMP_DIR"
}

# Run instrumentation test.
# Args:
#   $1: TEST_APK.
#   $2: EXTRA_FLAGS to be passed to run_instrumentation_tests.py.
function bb_run_instrumentation_test {
  local TEST_APK=${1}
  local EXTRA_FLAGS=${2}
  local INSTRUMENTATION_FLAGS="-vvv"
  INSTRUMENTATION_FLAGS+=" --test-apk ${TEST_APK}"
  INSTRUMENTATION_FLAGS+=" ${EXTRA_FLAGS}"
  build/android/run_instrumentation_tests.py ${INSTRUMENTATION_FLAGS}
}

# Run content shell instrumentation test on device.
function bb_run_instrumentation_tests {
  build/android/adb_install_content_shell
  local TEST_APK="content_shell_test/ContentShellTest-debug"
  # Use -I to install the test apk only on the first run.
  # TODO(bulach): remove the second once we have a Smoke test.
  bb_run_instrumentation_test ${TEST_APK} "-I -A Smoke"
  bb_run_instrumentation_test ${TEST_APK} "-I -A SmallTest"
  bb_run_instrumentation_test ${TEST_APK} "-A MediumTest"
  bb_run_instrumentation_test ${TEST_APK} "-A LargeTest"
}

# Zip and archive a build.
function bb_zip_build {
  echo "@@@BUILD_STEP Zip build@@@"
  python ../../../../scripts/slave/zip_build.py \
    --src-dir "$SRC_ROOT" \
    --exclude-files "lib.target,gen,android_webview,jingle_unittests" \
    --factory-properties "$FACTORY_PROPERTIES" \
    --build-properties "$BUILD_PROPERTIES"
}

# Download and extract a build.
function bb_extract_build {
  echo "@@@BUILD_STEP Download and extract build@@@"
  if [[ -z $FACTORY_PROPERTIES || -z $BUILD_PROPERTIES ]]; then
    return 1
  fi

  # When extract_build.py downloads an unversioned build it
  # issues a warning by exiting with large numbered return code
  # When it fails to download it build, it exits with return
  # code 1.  We disable halt on error mode and return normally
  # unless the python tool returns 1.
  (
  set +e
  python ../../../../scripts/slave/extract_build.py \
    --build-dir "$SRC_ROOT" \
    --build-output-dir "out" \
    --factory-properties "$FACTORY_PROPERTIES" \
    --build-properties "$BUILD_PROPERTIES"
  local extract_exit_code=$?
  if (( $extract_exit_code > 1 )); then
    echo "@@@STEP_WARNINGS@@@"
    return
  fi
  return $extract_exit_code
  )
}

# Reboot all phones and wait for them to start back up
# Does not break build if a phone fails to restart
function bb_reboot_phones {
  echo "@@@BUILD_STEP Rebooting phones@@@"
  (
  set +e
  cd $CHROME_SRC/build/android/pylib;
  for DEVICE in $(adb_get_devices); do
    python -c "import android_commands;\
        android_commands.AndroidCommands(device='$DEVICE').Reboot(True)" &
  done
  wait
  )
}

# Runs the license checker for the WebView build.
function bb_check_webview_licenses {
  echo "@@@BUILD_STEP Check licenses for WebView@@@"
  (
  set +e
  cd "${SRC_ROOT}"
  python android_webview/tools/webview_licenses.py scan
  local license_exit_code=$?
  if [[ license_exit_code -ne 0 ]]; then
    echo "@@@STEP_FAILURE@@@"
  fi
  return $license_exit_code
  )
}

# Retrieve a packed json property using python
function bb_get_json_prop {
  local JSON="$1"
  local PROP="$2"

  python -c "import json; print json.loads('$JSON').get('$PROP')"
}

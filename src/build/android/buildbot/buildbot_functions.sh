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

# Basic setup for all bots to run after a source tree checkout.
# Args:
#   $1: source root.
#   $2 and beyond: key value pairs which are parsed by bb_parse_args.
function bb_baseline_setup {
  SRC_ROOT="$1"
  # Remove SRC_ROOT param
  shift
  cd $SRC_ROOT

  echo "@@@BUILD_STEP Environment setup@@@"
  bb_parse_args "$@"

  local BUILDTOOL=$(bb_get_json_prop "$FACTORY_PROPERTIES" buildtool)
  if [[ $BUILDTOOL = ninja ]]; then
    export GYP_GENERATORS=ninja
  fi
  export GOMA_DIR=/b/build/goma
  . build/android/envsetup.sh

  local extra_gyp_defines="$(bb_get_json_prop "$FACTORY_PROPERTIES" \
     extra_gyp_defines)"
  export GYP_DEFINES+=" fastbuild=1 $extra_gyp_defines"
  if echo $extra_gyp_defines | grep -qE 'clang|asan'; then
    unset CXX_target
  fi

  adb kill-server
  adb start-server

  local build_path="${SRC_ROOT}/out/${BUILDTYPE}"
  local landmines_triggered_path="$build_path/.landmines_triggered"
  python "$SRC_ROOT/build/landmines.py"

  if [[ $BUILDBOT_CLOBBER || -f "$landmines_triggered_path" ]]; then
    echo "@@@BUILD_STEP Clobber@@@"

    if [[ -z $BUILDBOT_CLOBBER ]]; then
      echo "Clobbering due to triggered landmines: "
      cat "$landmines_triggered_path"
    else
      # Also remove all the files under out/ on an explicit clobber
      find "${SRC_ROOT}/out" -maxdepth 1 -type f -exec rm -f {} +
    fi

    # Sdk key expires, delete android folder.
    # crbug.com/145860
    rm -rf ~/.android
    rm -rf "$build_path"
    if [[ -e $build_path ]] ; then
      echo "Clobber appeared to fail?  $build_path still exists."
      echo "@@@STEP_WARNINGS@@@"
    fi
  fi
}

function bb_compile_setup {
  bb_setup_goma_internal
  # Should be called only after envsetup is done.
  gclient runhooks
}

# Setup goma.  Used internally to buildbot_functions.sh.
function bb_setup_goma_internal {
  export GOMA_API_KEY_FILE=${GOMA_DIR}/goma.key
  export GOMA_COMPILER_PROXY_DAEMON_MODE=true
  export GOMA_COMPILER_PROXY_RPC_TIMEOUT_SECS=300

  echo "Killing old goma processes"
  ${GOMA_DIR}/goma_ctl.sh stop || true
  killall -9 compiler_proxy || true

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
  bb_compile_setup

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

function bb_spawn_logcat_monitor_and_status {
  python build/android/device_status_check.py
  LOGCAT_DUMP_DIR="$CHROME_SRC/out/logcat"
  rm -rf "$LOGCAT_DUMP_DIR"
  python build/android/adb_logcat_monitor.py "$LOGCAT_DUMP_DIR" &
}

function bb_print_logcat {
  echo "@@@BUILD_STEP Logcat dump@@@"
  python build/android/adb_logcat_printer.py "$LOGCAT_DUMP_DIR"
}

# Run tests on an actual device.  (Better have one plugged in!)
function bb_run_unit_tests {
  build/android/run_tests.py --xvfb --verbose
}

# Run WebKit's test suites: webkit_unit_tests and TestWebKitAPI
function bb_run_webkit_unit_tests {
  if [[ $BUILDTYPE = Release ]]; then
    local BUILDFLAG="--release"
  fi
  bb_run_step build/android/run_tests.py --xvfb --verbose $BUILDFLAG \
      -s webkit_unit_tests
  bb_run_step build/android/run_tests.py --xvfb --verbose $BUILDFLAG \
      -s TestWebKitAPI
}

# Lint WebKit's TestExpectation files.
function bb_lint_webkit_expectation_files {
  echo "@@@BUILD_STEP webkit_lint@@@"
  bb_run_step python webkit/tools/layout_tests/run_webkit_tests.py \
    --lint-test-files \
    --chromium
}

# Run layout tests on an actual device.
function bb_run_webkit_layout_tests {
  echo "@@@BUILD_STEP webkit_tests@@@"
  local BUILDERNAME="$(bb_get_json_prop "$BUILD_PROPERTIES" buildername)"
  local BUILDNUMBER="$(bb_get_json_prop "$BUILD_PROPERTIES" buildnumber)"
  local MASTERNAME="$(bb_get_json_prop "$BUILD_PROPERTIES" mastername)"
  local RESULTSERVER=\
"$(bb_get_json_prop "$FACTORY_PROPERTIES" test_results_server)"

  bb_run_step python webkit/tools/layout_tests/run_webkit_tests.py \
      --no-show-results \
      --no-new-test-results \
      --full-results-html \
      --clobber-old-results \
      --exit-after-n-failures 5000 \
      --exit-after-n-crashes-or-timeouts 100 \
      --debug-rwt-logging \
      --results-directory "../layout-test-results" \
      --target "$BUILDTYPE" \
      --builder-name "$BUILDERNAME" \
      --build-number "$BUILDNUMBER" \
      --master-name "$MASTERNAME" \
      --build-name "$BUILDERNAME" \
      --platform=chromium-android \
      --test-results-server "$RESULTSERVER"
}

# Run experimental unittest bundles.
function bb_run_experimental_unit_tests {
  build/android/run_tests.py --xvfb --verbose -s android_webview_unittests
}

# Run findbugs.
function bb_run_findbugs {
  echo "@@@BUILD_STEP findbugs@@@"
  if [[ $BUILDTYPE = Release ]]; then
    local BUILDFLAG="--release-build"
  fi
  bb_run_step build/android/findbugs_diff.py $BUILDFLAG
  bb_run_step tools/android/findbugs_plugin/test/run_findbugs_plugin_tests.py \
    $BUILDFLAG
}

# Run a buildbot step and handle failure (failure will not halt build).
function bb_run_step {
  (
  set +e
  "$@"
  if [[ $? != 0 ]]; then
    echo "@@@STEP_FAILURE@@@"
  fi
  )
}

# Install a specific APK.
# Args:
#   $1: APK to be installed.
#   $2: APK_PACKAGE for the APK to be installed.
function bb_install_apk {
  local APK=${1}
  local APK_PACKAGE=${2}
  if [[ $BUILDTYPE = Release ]]; then
    local BUILDFLAG="--release"
  fi

  echo "@@@BUILD_STEP Install ${APK}@@@"
  python build/android/adb_install_apk.py --apk ${APK} \
      --apk_package ${APK_PACKAGE} ${BUILDFLAG}
}

# Run instrumentation tests for a specific APK.
# Args:
#   $1: APK to be installed.
#   $2: APK_PACKAGE for the APK to be installed.
#   $3: TEST_APK to run the tests against.
#   $4: TEST_DATA in format destination:source
function bb_run_all_instrumentation_tests_for_apk {
  local APK=${1}
  local APK_PACKAGE=${2}
  local TEST_APK=${3}
  local TEST_DATA=${4}

  # Install application APK.
  bb_install_apk ${APK} ${APK_PACKAGE}

  # Run instrumentation tests. Using -I to install the test apk.
  echo "@@@BUILD_STEP Run instrumentation tests ${TEST_APK}@@@"
  bb_run_step python build/android/run_instrumentation_tests.py \
      -vvv --test-apk ${TEST_APK} -I --test_data ${TEST_DATA}
}

# Run instrumentation tests for all relevant APKs on device.
function bb_run_instrumentation_tests {
  bb_run_all_instrumentation_tests_for_apk "ContentShell.apk" \
      "org.chromium.content_shell" "ContentShellTest" \
      "content:content/test/data/android/device_files"
  bb_run_all_instrumentation_tests_for_apk "ChromiumTestShell.apk" \
      "org.chromium.chrome.testshell" "ChromiumTestShellTest" \
      "chrome:chrome/test/data/android/device_files"
  bb_run_all_instrumentation_tests_for_apk "AndroidWebView.apk" \
      "org.chromium.android_webview" "AndroidWebViewTest" \
      "webview:android_webview/test/data/device_files"
}

# Run instrumentation tests for experimental APKs on device.
function bb_run_experimental_instrumentation_tests {
  echo "" # Can't have empty functions in bash.
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
    --build-dir "$SRC_ROOT/build" \
    --build-output-dir "../out" \
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
  if [[ $? -ne 0 ]]; then
    echo "@@@STEP_WARNINGS@@@"
  fi
  return 0
  )
}

# Retrieve a packed json property using python
function bb_get_json_prop {
  local JSON="$1"
  local PROP="$2"

  python -c "import json; print json.loads('$JSON').get('$PROP', '')"
}

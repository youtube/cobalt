#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# IMPORTANT! This script relies on "${HOME}/scratch/". This directory
# is made when it runs (and must not exist at runtime) and is left
# behind at termination (for you to save off or remove).
#
# For more fine-grained instructions, see:
# https://docs.google.com/document/d/1chTvr3fSofQNV_PDPEHRyUgcJCQBgTDOOBriW9gIm9M/edit?ts=5e9549a2#heading=h.fjdnrdg1gcty

set -e  # makes the script quit on any command failure
set -u  # unset variables are quit-worthy errors

PLATFORMS="${1:-win,android}"

SCRIPT_PATH=$(realpath $0)
REWRITER_SRC_DIR=$(dirname $SCRIPT_PATH)

COMPILE_DIRS=.
EDIT_DIRS=.
SCRATCH_DIR="${HOME}/scratch/"

# Make the scratch dir, relying on mkdir's natural fail-on-existing
# behavior (and prior `set -e` of this script).
mkdir "${SCRATCH_DIR}"

# Save llvm-build as it is about to be overwritten.
mv third_party/llvm-build third_party/llvm-build-upstream

# Build and test the rewriter.
echo "*** Building the rewriter ***"
time tools/clang/scripts/build.py \
    --with-android \
    --without-fuchsia \
    --extra-tools rewrite_raw_ptr_fields
tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

args_for_platform() {
    case "$1" in

    android)
        cat <<EOF
target_os = "android"
clang_use_chrome_plugins = false
is_chrome_branded = true
is_debug = false
dcheck_always_on = true
is_official_build = true
symbol_level = 1
use_goma = false
enable_remoting = true
ffmpeg_branding = "Chrome"
proprietary_codecs = true
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    win)
        cat <<EOF
target_os = "win"
clang_use_chrome_plugins = false
enable_precompiled_headers = false
is_chrome_branded = true
is_debug = false
dcheck_always_on = true
is_official_build = true
symbol_level = 1
use_goma = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    linux)
        cat <<EOF
target_os = "linux"
dcheck_always_on = true
is_chrome_branded = true
is_debug = false
is_official_build = true
use_goma = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    chromeos-lacros)
        cat <<EOF
target_os = "chromeos"
chromeos_is_browser_only = true
dcheck_always_on = true
is_chrome_branded = true
is_debug = false
is_official_build = true
use_goma = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    chromeos-ash)
        cat <<EOF
target_os = "chromeos"
chromeos_is_browser_only = false
dcheck_always_on = true
is_chrome_branded = true
is_debug = false
is_official_build = true
use_goma = false
chrome_pgo_phase = 0
force_enable_raw_ptr_exclusion = true
EOF
        ;;

    mac)
        cat <<EOF
target_os = "mac"
dcheck_always_on = true
is_chrome_branded = true
is_debug = false
is_official_build = true
use_goma = false
chrome_pgo_phase = 0
symbol_level = 1
force_enable_raw_ptr_exclusion = true
# crbug/1396061
enable_dsyms = false
# Can't exec Xcode `strip` binary
enable_stripping = false
EOF
        ;;

    *)
        echo "unknown platform"
        exit 1
        ;;
    esac
}

pre_process() {
    PLATFORM="$1"
    OUT_DIR="out/rewrite-$PLATFORM"

    mkdir -p "$OUT_DIR"
    args_for_platform "$PLATFORM" > "$OUT_DIR/args.gn"

    # Build generated files that a successful compilation depends on.
    echo "*** Preparing targets for $PLATFORM ***"
    gn gen $OUT_DIR
    time ninja -C $OUT_DIR -t targets all \
        | grep '^gen/.*\(\.h\|inc\|css_tokenizer_codepoints.cc\)' \
        | cut -d : -f 1 \
        | xargs -s $(expr $(getconf ARG_MAX) - 256) ninja -C $OUT_DIR

    TARGET_OS_OPTION=""
    if [ $PLATFORM = "win" ]; then
        TARGET_OS_OPTION="--target_os=win"
    fi

    # A preliminary rewriter run in a special mode that generates a list of fields
    # to ignore. These fields would likely lead to compiler errors if rewritten.
    echo "*** Generating the ignore list for $PLATFORM ***"
    time tools/clang/scripts/run_tool.py \
        $TARGET_OS_OPTION \
        --tool rewrite_raw_ptr_fields \
        --generate-compdb \
        -p $OUT_DIR \
        $COMPILE_DIRS > ~/scratch/rewriter-$PLATFORM.out
    cat ~/scratch/rewriter-$PLATFORM.out >> ~/scratch/rewriter.out
}

main_rewrite() {
    PLATFORM=$1
    OUT_DIR="out/rewrite-${PLATFORM}"

    TARGET_OS_OPTION=""
    if [ $PLATFORM = "win" ]; then
        TARGET_OS_OPTION="--target_os=win"
    fi

    # Main rewrite.
    echo "*** Running the main rewrite phase for $PLATFORM ***"
    time tools/clang/scripts/run_tool.py \
        $TARGET_OS_OPTION \
        --tool rewrite_raw_ptr_fields \
        --tool-arg=--exclude-fields="$HOME/scratch/combined-fields-to-ignore.txt" \
        -p $OUT_DIR \
        $COMPILE_DIRS > ~/scratch/rewriter-$PLATFORM.main.out
    cat ~/scratch/rewriter-$PLATFORM.main.out >> ~/scratch/rewriter.main.out
}

for PLATFORM in ${PLATFORMS//,/ }
do
    pre_process "$PLATFORM"
done

cat ~/scratch/rewriter.out \
    | sed '/^==== BEGIN FIELD FILTERS ====$/,/^==== END FIELD FILTERS ====$/{//!b};d' \
    | sort | uniq > ~/scratch/automated-fields-to-ignore.txt
cat ~/scratch/automated-fields-to-ignore.txt \
    tools/clang/rewrite_raw_ptr_fields/manual-fields-to-ignore.txt \
    | grep -v "base::FileDescriptorWatcher::Controller::watcher_" \
    > ~/scratch/combined-fields-to-ignore.txt

for PLATFORM in ${PLATFORMS//,/ }
do
    main_rewrite "$PLATFORM"
done

# Apply edits generated by the main rewrite.
echo "*** Applying edits ***"
cat ~/scratch/rewriter.main.out | \
    tools/clang/scripts/extract_edits.py | \
    tools/clang/scripts/apply_edits.py -p out/rewrite-win $EDIT_DIRS

# Format sources, as many lines are likely over 80 chars now.
echo "*** Formatting ***"
time git cl format

# Restore llvm-build. Without this, your future builds will be painfully slow.
rm -r -f third_party/llvm-build
mv third_party/llvm-build-upstream third_party/llvm-build

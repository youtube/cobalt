#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Buildbot annotator script for trybots.  Compile only.

BB_DIR="$(dirname $0)"
BB_SRC_ROOT="$(cd  "$BB_DIR/../../.."; pwd)"
. "$BB_DIR/buildbot_functions.sh"

# SHERIFF: if you need to quickly turn "android_dbg" trybots green,
# uncomment the next line (and send appropriate email out):
## bb_force_bot_green_and_exit
# You will also need to change buildbot_try_tester.sh

bb_baseline_setup "$BB_SRC_ROOT" "$@"
bb_compile
bb_zip_build

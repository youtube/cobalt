#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Buildbot annotator script for the WebKit latest WebKit tester on the
# WebKit canary waterfall.

BB_DIR="$(dirname $0)"
BB_SRC_ROOT="$(cd  "$BB_DIR/../../.."; pwd)"
. "$BB_DIR/buildbot_functions.sh"

bb_baseline_setup "$BB_SRC_ROOT" "$@"
bb_spawn_logcat_monitor_and_status
bb_extract_build
bb_reboot_phones
# TODO(peter@): Replace this with a test call
echo "@@@BUILD_STEP TODO Run WebKit tests@@@"
bb_print_logcat

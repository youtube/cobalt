#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Buildbot annotator script for the FYI waterfall builder.
# Compiler and zip the build.

# SHERIFF: there should be no need to disable this bot.
# The FYI waterfall does not close the tree.

BB_DIR="$(dirname $0)"
BB_SRC_ROOT="$(cd  "$BB_DIR/../../.."; pwd)"
. "$BB_DIR/buildbot_functions.sh"

bb_baseline_setup "$BB_SRC_ROOT" "$@"
bb_check_webview_licenses
bb_compile
bb_compile_experimental
bb_zip_build

#!/bin/bash -ex
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Buildbot annotator entry for trybot mirroring fyi tester
exec "$(dirname $0)/bb_fyi_tester.sh" "$@"

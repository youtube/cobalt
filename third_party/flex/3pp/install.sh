#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euxo pipefail

PREFIX="$1"

./configure --host="$CROSS_TRIPLE" --prefix="$PREFIX"
make install "-j$(nproc)"

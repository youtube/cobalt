#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

treeroot="$(dirname "$0")/.."
cd "${treeroot}"

echo "Applying brkitr.patch"
patch -p1 < cast/brkitr.patch || { echo "failed to patch" >&2; exit 1; }

# One of the purposes of this patch is to reduce the binary size by excluding
# `cjdict`, which is one of the biggest resources.
#
# On the other hand, the AdaBoost ML line break engine requires `cjdict` to
# exist, regardless of the content. To enable the AdaBoost ML line break
# engine, this patch makes `cjdcit` empty instead of excluding.
#
# It can't be really empty though, as the build tool fails if it's empty.
# Instead, this patch makes it just one entry.
CJDICT="${treeroot}/source/data/brkitr/dictionaries/cjdict.txt"
CJDICT_TMP="${CJDICT}.tmp"
grep -E "^[^#[:space:]]+[[:space:]]+[[:digit:]]+" "$CJDICT" |
  tail -1 > "$CJDICT_TMP"
mv "$CJDICT_TMP" "$CJDICT"

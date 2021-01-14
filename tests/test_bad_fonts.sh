#!/bin/bash

# Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

test "x$top_srcdir" = x && top_srcdir=.
test "x$top_builddir" = x && top_builddir=.

# Usage: ./test_bad_fonts.sh [ttf_or_otf_file_name]

BASE_DIR=$top_srcdir/tests/fonts/bad/
CHECKER=$top_builddir/ots-sanitize$EXEEXT

if [ ! -x "$CHECKER" ] ; then
  echo "$CHECKER is not found."
  exit 1
fi

if [ $# -eq 0 ] ; then
  # No font file is specified. Apply this script to all TT/OT files under the
  # BASE_DIR.
  if [ ! -d $BASE_DIR ] ; then
    echo "$BASE_DIR does not exist."
    exit 1
  fi

  FONTS=$(find $BASE_DIR -type f)
  # Recursively call this script.
  FAILS=0
  IFS=$'\n'
  for f in $FONTS; do
    $0 "$f"
    FAILS=$((FAILS+$?))
  done

  if [ $FAILS != 0 ]; then
    echo "$FAILS fonts failed."
    exit 1
  else
    echo "All fonts passed"
    exit 0
  fi
fi

if [ $# -gt 1 ] ; then
  echo "Usage: $0 [ttf_or_otf_file_name]"
  exit 1
fi

# Confirm that the bad font file is rejected by OTS.
$CHECKER "$1"
RET=$?
if [ $RET != 0 ]; then
  echo "PASSED: $1"
  exit 0
else
  echo "FAILED: $1"
  exit 1
fi

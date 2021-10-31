#!/bin/bash

# Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

test "x$top_srcdir" = x && top_srcdir=.
test "x$top_builddir" = x && top_builddir=.

# Usage: ./test_good_fonts.sh [ttf_or_otf_file_name]

BASE_DIR=$top_srcdir/tests/fonts/good/
BLACKLIST=$top_srcdir/tests/BLACKLIST.txt
CHECKER=$top_builddir/ots-idempotent$EXEEXT

if [ ! -r "$BLACKLIST" ] ; then
  echo "$BLACKLIST is not found."
  exit 1
fi

if [ ! -x "$CHECKER" ] ; then
  echo "$CHECKER is not found."
  exit 1
fi

if [ $# -eq 0 ] ; then
  # No font file is specified. Apply this script to all TT/OT files we can find
  # on the system.

  if [ ! -d $BASE_DIR ] ; then
    echo "$BASE_DIR does not exist."
    exit 1
  fi

  if $(command fc-list &>/dev/null);
  then
    CFF=$(fc-list --format="%{file}\n" :fontformat=CFF)
    TTF=$(fc-list --format="%{file}\n" :fontformat=TrueType)
    FONTS="$CFF"$'\n'"$TTF"
  else
    # Mac OS X
    # TODO: Support Cygwin.
    OSX_BASE_DIR="/Library/Fonts/ /System/Library/Fonts/"
    FONTS=$(find $OSX_BASE_DIR -type f -name '*tf' -o -name '*tc')
  fi

  FONTS="$FONTS"$'\n'"$(find $BASE_DIR -type f)"

  # Recursively call this script.
  FAILS=0
  IFS=$'\n'
  for f in $FONTS; do
    if [[ $f == *.dfont ]]; then continue; fi # Ignore .dfont’s
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

# Check the font file using idempotent if the font is not blacklisted.
BASE=`basename "$1"`
SKIP=`egrep -i -e "^$BASE" "$BLACKLIST"`

if [ "x$SKIP" = "x" ]; then
  $CHECKER "$1"
  RET=$?
  if [ $RET != 0 ]; then
    echo "FAILED: $1"
  else
    echo "PASSED: $1"
  fi
  exit $RET
fi

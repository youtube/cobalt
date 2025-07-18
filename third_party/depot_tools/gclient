#!/usr/bin/env bash
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

base_dir=$(dirname "$0")

if [[ "#grep#fetch#cleanup#diff#help#getdep#root#setdep#recurse#--help#" != *"#$1#"* ]]; then
  # Shall skip authomatic update?
  if [[ $DEPOT_TOOLS_UPDATE != 0 ]]; then
    "$base_dir"/update_depot_tools "$@"
    case $? in
      123)
        # msys environment was upgraded, need to quit.
        exit 0
        ;;
      0)
        ;;
      *)
        exit $?
    esac
  fi
fi

# Ensure that "depot_tools" is somewhere in PATH so this tool can be used
# standalone, but allow other PATH manipulations to take priority.
PATH=$PATH:$base_dir

if [[ $GCLIENT_PY3 == 1 ]]; then
  # Explicitly run on Python 3
  PYTHONDONTWRITEBYTECODE=1 exec vpython3 "$base_dir/gclient.py" "$@"
elif [[ $GCLIENT_PY3 == 0 ]]; then
  # Explicitly run on Python 2
  PYTHONDONTWRITEBYTECODE=1 exec vpython "$base_dir/gclient.py" "$@"
else
  # Run on Python 3, allows default to be flipped.
  PYTHONDONTWRITEBYTECODE=1 exec vpython3 "$base_dir/gclient.py" "$@"
fi

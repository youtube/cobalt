#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys

# Add //build to sys.path to import action_helpers
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'build'))
import action_helpers


def main():
  if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <src> <dst>", file=sys.stderr)
    return 1

  src = sys.argv[1]
  dst = sys.argv[2]

  dst = os.path.normpath(dst)
  with action_helpers.atomic_output(dst) as f:
    with open(src, 'rb') as fsrc:
      shutil.copyfileobj(fsrc, f)
    shutil.copymode(src, f.name)
  return 0


if __name__ == '__main__':
  sys.exit(main())

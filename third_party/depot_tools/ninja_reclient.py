#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is a wrapper around the ninja.py script that also
handles the client lifecycle safely. It will automatically start
reproxy before running ninja and stop reproxy when ninja stops
for any reason eg. build completes, keyboard interupt etc."""

import sys

import ninja
import reclient_helper


def main(argv):
  with reclient_helper.build_context(argv) as ret_code:
    if ret_code:
      return ret_code
    try:
      return ninja.main(argv)
    except KeyboardInterrupt:
      return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv))

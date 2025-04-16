# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A script to setup YTLR performance tests, mainly setting environment
    variables for finding modules.
"""

import os
import sys
# from $CHROMIUM_SRC/cobalt/tools/performance
REPOSITORY_ROOT = os.path.abspath(os.path.abspath(__file__) + '/../../../../')
sys.path.append(os.path.join(REPOSITORY_ROOT, 'build', 'android'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'devil'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'telemetry'))
# Disabling warning about imports being at the top.
# We need this so the ENV vars can be found!
# pylint: disable=wrong-import-position
from devil_script import PerfTesting


def main():
  perf_test = PerfTesting()
  perf_test.RunTests()


if __name__ == '__main__':
  main()

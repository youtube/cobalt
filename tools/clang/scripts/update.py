#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Windows can't run .sh files, so this is a small python wrapper around
# update.sh.

import os
import subprocess
import sys

if __name__ == '__main__':
  if sys.platform in ['win32', 'cygwin']:
    sys.exit(0)
  subprocess.call(
      [os.path.join(os.path.dirname(__file__), 'update.sh')] +  sys.argv[1:])

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import absolute_import

import os
import sys

_CHROME_SRC = os.path.join(
    os.path.abspath(os.path.dirname(__file__)), '..', '..', '..')

_PEXPECT_PATH = os.path.join(_CHROME_SRC, 'third_party', 'pexpect')
if _PEXPECT_PATH not in sys.path:
  sys.path.append(_PEXPECT_PATH)

from pexpect import *

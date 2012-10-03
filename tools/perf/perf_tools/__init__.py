# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A library for Chrome GPU test code."""
import os
import sys

def Init():
  crc_path = os.path.join(os.path.dirname(__file__),
                          '..', '..', 'chrome_remote_control')
  absolute_crc_path = os.path.abspath(crc_path)
  sys.path.append(absolute_crc_path)

Init()

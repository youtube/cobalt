#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Extracts a single file from a CAB archive.

import os
import subprocess
import sys

if len(sys.argv) != 4:
  print 'Usage: extract_from_cab.py cab_path archived_file output_dir'
  sys.exit(1)

[cab_path, archived_file, output_dir] = sys.argv[1:]

# Invoke the Windows expand utility to extract the file.
level = subprocess.call(['expand', cab_path, '-F:' + archived_file, output_dir])
if level != 0:
  sys.exit(level)

# The expand utility preserves the modification date and time of the archived
# file. Touch the extracted file. This helps build systems that compare the
# modification times of input and output files to determine whether to do an
# action.
os.utime(os.path.join(output_dir, archived_file), None)

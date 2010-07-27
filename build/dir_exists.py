#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys

sys.stdout.write(str(os.path.isdir(sys.argv[1])))
sys.exit(0)

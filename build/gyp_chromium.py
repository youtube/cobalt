# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a workaround for multiprocessing on Windows. Importing in Python on
# Windows doesn't search for imports that don't end in .py (and aren't
# directories with an __init__.py). So, add this wrapper to avoid having
# people change their command line to add a .py when running gyp_chromium.
__import__('gyp_chromium')

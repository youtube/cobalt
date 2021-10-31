# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include this file for a main() function that simply instantiates and runs a
# base::TestSuite.
{
  'dependencies': [
    '<(DEPTH)/base/base.gyp:test_support_base',
    '<(DEPTH)/testing/gmock.gyp:gmock',
    '<(DEPTH)/testing/gtest.gyp:gtest',
  ],
  'sources': [
    '<(DEPTH)/base/test/run_all_unittests.cc',
  ],
}

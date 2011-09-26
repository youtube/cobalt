# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is all.gyp file for Android to prevent breakage in Android and other
# platform; It will be churning a lot in the short term and eventually be merged
# into all.gyp.
#
# Currently only base_unittests is supported.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'util/build_util.gyp:*',
      ],
    }, # target_name: All
    {
      # The current list of tests for android.
      # This is temporary until the full set supported.
      'target_name': 'android_builder_tests',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_unittests',
      ],
    },
  ],  # targets
}

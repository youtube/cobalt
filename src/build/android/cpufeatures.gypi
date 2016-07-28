# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Depend on the Android NDK's cpu feature detection. The WebView build is part
# of the system and the library already exists; for the normal build there is a
# gyp file in the checked-in NDK to build it.
{
  'conditions': [
    ['android_build_type != 0', {
      'libraries': [
        'cpufeatures.a'
      ],
    }, {
      'dependencies': [
        '<(android_ndk_root)/android_tools_ndk.gyp:cpu_features',
      ],
    }],
  ],
}

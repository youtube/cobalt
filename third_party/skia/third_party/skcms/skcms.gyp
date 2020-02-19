# Copyright 2018 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'skcms',
      'type': 'static_library',
      'toolsets': ['target'],
      'defines': [
        # Starboard doesn't have APIs for vector registers.
        # Use portable code instead.
        'SKCMS_PORTABLE',
      ],
      'sources': [
        'skcms.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/skia/include/third_party/skcms',
      ],
    },
  ],
}

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'dynamic_annotations',
      'type': '<(library)',
      'msvs_guid': 'EF3AD1A1-5FA6-4B70-9CCC-F5AE4C6D0892',
      'include_dirs': [
        '../../../',
      ],
      'sources': [
        'dynamic_annotations.c',
        'dynamic_annotations.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "win"', {
      'targets': [
        {
          'target_name': 'dynamic_annotations_win64',
          'type': '<(library)',
          'msvs_guid': 'E8055455-0065-427B-9461-34A16FAD1973',
          # We can't use dynamic_annotations target for win64 build since it is
          # a 32-bit library.
          # TODO(gregoryd): merge with dynamic_annotations when
          # the win32/64 targets are merged.
          'include_dirs': [
              '../../../',
          ],
          'sources': [
            'dynamic_annotations.c',
            'dynamic_annotations.h',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}

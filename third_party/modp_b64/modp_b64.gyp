# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'modp_b64',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'sources': [
        'modp_b64.cc',
        'modp_b64.h',
        'modp_b64_data.h',
      ],
      'include_dirs': [
        '../..',
      ],
      'conditions': [
        ['OS!="starboard" and (target_arch=="ps3" or target_arch=="wiiu" or target_arch=="xb360")', {
          'defines': [
            'WORDS_BIGENDIAN'
          ]
        }]
      ]
    },
  ],
}

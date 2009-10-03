# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets' : [ 
    {
      'target_name': 'image_diff',
      'type': 'executable',
      'msvs_guid': '50B079C7-CD01-42D3-B8C4-9F8D9322E822',
      'dependencies': [
        '../../app/app.gyp:app_base',
        '../../base/base.gyp:base',
      ],
      'sources': [
        'image_diff.cc',
      ],
    },
  ],
}


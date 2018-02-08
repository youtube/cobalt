# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'dynamic_annotations',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'include_dirs': [
        '../../../',
      ],
      'sources': [
        'dynamic_annotations.c',
        'dynamic_annotations.h',
        '../valgrind/valgrind.h',
      ],
    },
  ],
}

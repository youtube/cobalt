# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../native_client/build/untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'shared_memory_support_untrusted',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nlib_target': 'libshared_memory_support_untrusted.a',
            'build_glibc': 0,
            'build_newlib': 1,
          },
          'dependencies': [
            '../native_client/tools.gyp:prep_toolchain',
            '../base/base_untrusted.gyp:base_untrusted',
          ],
          'defines': [
            'MEDIA_IMPLEMENTATION',
          ],
          'include_dirs': [
            '..',
          ],
          'includes': [
            'shared_memory_support.gypi',
          ],
          'sources': [
            '<@(shared_memory_support_sources)',
          ],
        },
      ],
    }],
  ],
}

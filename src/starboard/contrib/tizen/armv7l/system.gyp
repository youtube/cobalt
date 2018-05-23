# Copyright (c) 2018 Samsung Electronics. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'pkg-config': 'pkg-config',
  },
  'targets': [
    {
      'target_name': 'libpulse-simple',
      'type': 'none',
      'direct_dependent_settings': {
        'cflags': [
          '<!@(<(pkg-config) --cflags libpulse-simple)',
        ],
      },
      'link_settings': {
        'ldflags': [
          '<!@(<(pkg-config) --libs-only-L --libs-only-other libpulse-simple)',
        ],
        'libraries': [
          '<!@(<(pkg-config) --libs-only-l libpulse-simple)',
        ],
      },
    }, # libpulse
  ],
}

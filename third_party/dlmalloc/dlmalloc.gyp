# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'dlmalloc',
      'type': '<(library)',
      'sources': [
        'dlmalloc.c',
      ],
      'include_dirs': [
      ],
      'conditions': [
        ['target_os=="win"', {
          # Compile dlmalloc.c as C++ on MSVC.
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': ['/TP'],
            },
          },
        },],
        ['target_arch=="android"', {
          'cflags': [
            '-std=c99',
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

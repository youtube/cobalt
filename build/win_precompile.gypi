# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include this file to make targets in your .gyp use the default
# precompiled header on Windows.

{
  'conditions': [
    # Restricted to VS 2010 until GYP also supports suppressing
    # precompiled headers on .c files in VS 2008.
    ['OS=="win" and (MSVS_VERSION=="2010" or MSVS_VERSION=="2010e")', {
      'target_defaults': {
        'msvs_precompiled_header': '<(DEPTH)/build/precompile.h',
        'msvs_precompiled_source': '<(DEPTH)/build/precompile.cc',
        'sources': ['<(DEPTH)/build/precompile.cc'],
      },
    }],
  ],
}

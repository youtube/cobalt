# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # This target is only used by the media stack which still has references
      # to the utility classes in the following header files.
      # The header files simply introduce the classes defined in cobalt/math to
      # namespace gfx.
      'target_name': 'ui',
      'type': '<(component)',
      'dependencies': [
        '../cobalt/math/math.gyp:math',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'ui/gfx/rect.h',
        'ui/gfx/rect_f.h',
        'ui/gfx/size.h',
      ],
    }
  ],
}

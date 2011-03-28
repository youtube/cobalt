# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target that will have one or more
# uses of grit_action.gypi. To use this the following variables need to be
# defined:
#   grit_out_dir: string: the output directory path

# NOTE: This file is optional, not all targets that use grit include it, some
# do their own custom directives instead.
{
  'direct_dependent_settings': {
    'include_dirs': [
      '<(grit_out_dir)',
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'dependencies': ['<(DEPTH)/build/win/system.gyp:cygwin'],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into an action to invoke grit in a
# consistent manner. To use this the following variables need to be
# defined:
#   grit_grd_file: string: grd file path
#   grit_out_dir: string: the output directory path

# It would be really nice to do this with a rule instead of actions, but it
# would need to determine inputs and outputs via grit_info on a per-file
# basis. GYP rules don’t currently support that. They could be extended to
# do this, but then every generator would need to be updated to handle this.

{
  'variables': {
    'grit_info_cmd': ['python', '<(DEPTH)/tools/grit/grit_info.py',
                      '<@(grit_defines)'],
    'grit_cmd': ['python', '<(DEPTH)/tools/grit/grit.py'],
  },
  'inputs': [
    '<!@(<(grit_info_cmd) --inputs <(grit_grd_file))',
  ],
  'outputs': [
    '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(grit_grd_file))',
  ],
  'action': ['<@(grit_cmd)',
             '-i', '<(grit_grd_file)', 'build',
             '-o', '<(grit_out_dir)',
             '<@(grit_defines)' ],
  'message': 'Generating resources from <(grit_grd_file)',
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

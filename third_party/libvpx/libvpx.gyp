# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # libvpx_lib is currently not being used since we use libvpx inside
    # libavcodec. Keeping this just in case we need this later.
    {
      'target_name': 'libvpx_lib',
      'type': 'none',
      'variables': {
        'libvpx_lib': 'libvpx.a',
      },
      'conditions': [
        # This section specifies the folder for looking for libvpx.a.
        #
        ['OS=="linux" and target_arch=="ia32"', {
          'variables': {
            'libvpx_path': 'lib/linux/ia32',
          },
        }],
        ['OS=="linux" and target_arch=="x64"', {
          'variables': {
            'libvpx_path': 'lib/linux/x64',
          },
        }],
        ['OS=="linux" and target_arch=="arm" and arm_neon==1', {
          'variables': {
            'libvpx_path': 'lib/linux/arm-neon',
          },
        }],
        ['OS=="linux" and target_arch=="arm" and arm_neon==0', {
          'variables': {
            'libvpx_path': 'lib/linux/arm',
          },
        }],
        ['OS=="win"', {
          'variables': {
            'libvpx_path': 'lib/win/ia32',
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'libvpx_path': 'lib/mac/ia32',
          },
        }],
      ],
      'actions': [
        {
          'action_name': 'copy_lib',
          'inputs': [
            '<(libvpx_path)/<(libvpx_lib)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/<(libvpx_lib)',
          ],
          'action': [
            'cp',
            '<(libvpx_path)/<(libvpx_lib)',
            '<(SHARED_INTERMEDIATE_DIR)/<(libvpx_lib)',
          ],
          'message': 'Copying libvpx.a into <(SHARED_INTERMEDIATE_DIR)',
        },
      ],
      'all_dependent_settings': {
        'link_settings': {
          'libraries': [
            '<(SHARED_INTERMEDIATE_DIR)/<(libvpx_lib)',
          ],
        },
      },
    },
    {
      'target_name': 'libvpx_include',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
    }
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:

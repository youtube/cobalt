# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      # Target for creating common output build directories. Creating output
      # dirs beforehand ensures that build scripts can assume these folders to
      # exist and there are no race conditions resulting from build scripts
      # trying to create these directories.
      # The build/java.gypi target depends on this target.
      'target_name': 'build_output_dirs',
      'type': 'none',
      'actions': [
        {
          'action_name': 'create_java_output_dirs',
          'variables' : {
          'output_dirs' : [
            '<(PRODUCT_DIR)/apks',
            '<(PRODUCT_DIR)/lib.java',
            '<(PRODUCT_DIR)/test.lib.java',
           ]
          },
          'inputs' : [],
          # By not specifying any outputs, we ensure that this command isn't
          # re-run when the output directories are touched (i.e. apks are
          # written to them).
          'outputs': [''],
          'action': [
            'mkdir',
            '-p',
            '<@(output_dirs)',
          ],
        },
      ],
    }, # build_output_dirs
  ], # targets
}

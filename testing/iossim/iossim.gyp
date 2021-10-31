# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'iossim',
            'type': 'executable',
            'variables': {
              'developer_dir': '<!(xcode-select -print-path)',
              'iphone_sim_path': '<(developer_dir)/Platforms/iPhoneSimulator.platform/Developer/Library/PrivateFrameworks',
              'other_frameworks_path': '<(developer_dir)/../OtherFrameworks'
            },
            'dependencies': [
              'third_party/class-dump/class-dump.gyp:class-dump',
            ],
            'include_dirs': [
              '<(INTERMEDIATE_DIR)/iossim',
            ],
            'sources': [
              'iossim.mm',
              '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h',
            ],
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
            'actions': [
              {
                'action_name': 'generate_iphone_sim_header',
                'inputs': [
                  '<(iphone_sim_path)/iPhoneSimulatorRemoteClient.framework/Versions/Current/iPhoneSimulatorRemoteClient',
                  '<(PRODUCT_DIR)/class-dump',
                ],
                'outputs': [
                  '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h'
                ],
                'action': [
                  # Actions don't provide a way to redirect stdout, so a custom
                  # script is invoked that will execute the first argument and write
                  # the output to the file specified as the second argument.
                  './redirect-stdout.sh',
                  '<(PRODUCT_DIR)/class-dump -CiPhoneSimulator <(iphone_sim_path)/iPhoneSimulatorRemoteClient.framework',
                  '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h',
                ],
                'message': 'Generating header',
              },
            ],
          },
        ],
      }, {  # else, OS == "ios"
        'variables': {
          'ninja_output_dir': 'ninja-iossim',
          'ninja_product_dir':
            '<(DEPTH)/xcodebuild/<(ninja_output_dir)/<(CONFIGURATION_NAME)',
        },
        # Generation is done via two actions: (1) compiling the executable with
        # ninja, and (2) copying the executable into a location that is shared
        # with other projects. These actions are separated into two targets in
        # order to be able to specify that the second action should not run
        # until the first action finishes (since the ordering of multiple
        # actions in one target is defined only by inputs and outputs, and it's
        # impossible to set correct inputs for the ninja build, so setting all
        # the inputs and outputs isn't an option).
        'targets': [
          {
            'target_name': 'compile_iossim',
            'type': 'none',
            'variables': {
              # Gyp to rerun
              're_run_targets': [
                 'testing/iossim/iossim.gyp',
              ],
            },
            'includes': ['../../build/ios/mac_build.gypi'],
            'actions': [
              {
                'action_name': 'compile iossim',
                'inputs': [],
                'outputs': [],
                'action': [
                  '<@(ninja_cmd)',
                  'iossim',
                ],
                'message': 'Generating the iossim executable',
              },
            ],
          },
          {
            'target_name': 'iossim',
            'type': 'none',
            'dependencies': [
              'compile_iossim',
            ],
            'actions': [
              {
                'action_name': 'copy iossim',
                'inputs': [
                  # TODO(ios): It should be possible to define the input, but
                  # adding it causes gyp to complain about duplicate id.
                  # '<(ninja_product_dir)/iossim',
                ],
                'outputs': [
                  '<(DEPTH)/xcodebuild/<(CONFIGURATION_NAME)/iossim',
                ],
                'action': [
                  'cp',
                  '<(ninja_product_dir)/iossim',
                  '<(DEPTH)/xcodebuild/<(CONFIGURATION_NAME)/iossim',
                ],
              },
            ],
          },
        ],
      },
    ],
  ],
}

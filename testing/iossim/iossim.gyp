# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'iphone_sim_path': '$(DEVELOPER_DIR)/Platforms/iPhoneSimulator.platform/Developer/Library/PrivateFrameworks',
    'other_frameworks_path': '$(DEVELOPER_DIR)/../OtherFrameworks'
  },
  'targets': [
    {
      'target_name': 'iossim',
      'type': 'executable',
      'dependencies': [
        '<(DEPTH)/testing/iossim/third_party/class-dump/class-dump.gyp:class-dump',
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
        '<(iphone_sim_path)/iPhoneSimulatorRemoteClient.framework',
      ],
      'mac_framework_dirs': [
        '<(iphone_sim_path)',
      ],
      'xcode_settings': {
        'LD_RUNPATH_SEARCH_PATHS': [
          '<(iphone_sim_path)',
          '<(other_frameworks_path)',
        ]
      },
      'actions': [
        {
          'action_name': 'generate_iphone_sim_header',
          'inputs': [
            '<(iphone_sim_path)/iPhoneSimulatorRemoteClient.framework/Versions/Current/iPhoneSimulatorRemoteClient',
            '$(BUILD_DIR)/$(CONFIGURATION)/class-dump',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h'
          ],
          'action': [
            # Actions don't provide a way to redirect stdout, so a custom
            # script is invoked that will execute the first argument and write
            # the output to the file specified as the second argument.
            '<(DEPTH)/testing/iossim/redirect-stdout.sh',
            '$(BUILD_DIR)/$(CONFIGURATION)/class-dump -CiPhoneSimulator <(iphone_sim_path)/iPhoneSimulatorRemoteClient.framework',
            '<(INTERMEDIATE_DIR)/iossim/iPhoneSimulatorRemoteClient.h',
          ],
          'message': 'Generating header',
        },
      ],
    },
  ],
}

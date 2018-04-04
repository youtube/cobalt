# Copyright 2018 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
  'variables': {
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'overlay_info',
      'type': 'static_library',
      'sources': [
        'overlay_info_registry.cc',
        'overlay_info_registry.h',
        'qr_code_overlay.cc',
        'qr_code_overlay.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/third_party/QR-Code-generator/qr_code_generator.gyp:qr_code_generator',
      ],
    },

    {
      'target_name': 'overlay_info_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'overlay_info_registry_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/overlay_info/overlay_info.gyp:overlay_info',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },

    {
      'target_name': 'overlay_info_test_deploy',
      'type': 'none',
      'dependencies': [
        'overlay_info_test',
      ],
      'variables': {
        'executable_name': 'overlay_info_test',
      },
      'includes': [ '../../starboard/build/deploy.gypi' ],
    },
  ],
}

# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
  'targets': [
    {
      'target_name': 'renderer_backend',
      'type': 'static_library',
      'sources': [
        'graphics_context.cc',
        'graphics_context.h',
        'render_target.cc',
        'render_target.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/renderer/backend/starboard/platform_backend.gyp:renderer_platform_backend',
      ],
      'conditions': [
        ['enable_map_to_mesh != -1', {
          'defines' : [
            'ENABLE_MAP_TO_MESH=<(enable_map_to_mesh)',
          ],
        }],
      ],
    },
    {
      'target_name': 'graphics_system_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'graphics_system_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/cobalt/renderer/backend/backend.gyp:renderer_backend',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
    },
    {
      'target_name': 'graphics_system_test_deploy',
      'type': 'none',
      'dependencies': [
        'graphics_system_test',
      ],
      'variables': {
        'executable_name': 'graphics_system_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}

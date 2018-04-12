# Copyright 2017 Google Inc. All Rights Reserved.
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
      'target_name': 'page_visibility',
      'type': 'static_library',
      'sources': [
        'page_visibility_state.cc',
        'page_visibility_state.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
      ],
      # This target doesn't generate any headers, but it exposes generated
      # header files through this module's public header files. So mark this
      # target as a hard dependency to ensure that any dependent targets wait
      # until this target (and its hard dependencies) are built.
      'hard_dependency': 1,
      'export_dependent_settings': [
        # Additionally, ensure that the include directories for generated
        # headers are put on the include directories for targets that depend
        # on this one.
        '<(DEPTH)/cobalt/browser/browser_bindings_gen.gyp:generated_types',
      ]
    },
    {
      'target_name': 'page_visibility_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'page_visibility_state_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'page_visibility',
      ],
    },
    {
      'target_name': 'page_visibility_test_deploy',
      'type': 'none',
      'dependencies': [
        'page_visibility_test',
      ],
      'variables': {
        'executable_name': 'page_visibility_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}

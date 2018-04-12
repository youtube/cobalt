# Copyright 2015 Google Inc. All Rights Reserved.
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
      'target_name': 'web_animations',
      'type': 'static_library',
      'sources': [
        'animatable.h',
        'animation.cc',
        'animation.h',
        'animation_effect_read_only.h',
        'animation_effect_timing_read_only.cc',
        'animation_effect_timing_read_only.h',
        'animation_set.cc',
        'animation_set.h',
        'animation_timeline.cc',
        'animation_timeline.h',
        'baked_animation_set.cc',
        'baked_animation_set.h',
        'keyframe.h',
        'keyframe_effect_read_only.cc',
        'keyframe_effect_read_only.h',
        'timed_task_queue.cc',
        'timed_task_queue.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:cssom',
      ],
    },

    {
      'target_name': 'web_animations_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'animation_test.cc',
        'animation_effect_timing_read_only_test.cc',
        'keyframe_effect_read_only_test.cc',
      ],
      'dependencies': [
        'web_animations',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:cssom',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },

    {
      'target_name': 'web_animations_test_deploy',
      'type': 'none',
      'dependencies': [
        'web_animations_test',
      ],
      'variables': {
        'executable_name': 'web_animations_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}

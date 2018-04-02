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

# This is a sample sandbox application for experimenting with the Cobalt
# render tree/renderer interface.

{
  'targets': [
    {
      # Contains code to build a demonstration scene used within benchmarking
      # code along with the sandbox application code.  Is useful in general
      # wherever an interesting demo render_tree is required.
      'target_name': 'scenes',
      'type': 'static_library',
      'sources': [
        'all_scenes_combined_scene.cc',
        'all_scenes_combined_scene.h',
        'growing_rect_scene.cc',
        'growing_rect_scene.h',
        'image_wrap_scene.cc',
        'image_wrap_scene.h',
        'marquee_scene.cc',
        'marquee_scene.h',
        'scaling_text_scene.cc',
        'scaling_text_scene.h',
        'scene_helpers.cc',
        'scene_helpers.h',
        'spinning_sprites_scene.cc',
        'spinning_sprites_scene.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:render_tree',
        '<(DEPTH)/cobalt/renderer/test/png_utils/png_utils.gyp:png_utils',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
        'scenes_copy_test_data',
      ],
    },
    {
      'target_name': 'scenes_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          'demo_image.png',
        ],
        'content_test_output_subdir': 'test/scenes',
      },
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },
  ],
}

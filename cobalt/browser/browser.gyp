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
    'cobalt_code': 1,
  },
  'targets': [
    {
      'target_name': 'browser',
      'type': 'static_library',
      'sources': [
        'application.cc',
        'application.h',
        'browser_module.cc',
        'browser_module.h',
        'debug_console.cc',
        'debug_console.h',
        'h5vcc_url_handler.cc',
        'h5vcc_url_handler.h',
        'render_tree_combiner.cc',
        'render_tree_combiner.h',
        'resource_provider_array_buffer_allocator.cc',
        'resource_provider_array_buffer_allocator.h',
        'splash_screen.cc',
        'splash_screen.h',
        'switches.cc',
        'switches.h',
        'trace_manager.cc',
        'trace_manager.h',
        'url_handler.cc',
        'url_handler.h',
        'web_module.cc',
        'web_module.h',
        'web_module_stat_tracker.cc',
        'web_module_stat_tracker.h',
      ],
      'defines': [
        'COBALT_IMAGE_CACHE_SIZE_IN_BYTES=<(image_cache_size_in_bytes)',
        'COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES=<(remote_typeface_cache_size_in_bytes)',
        'COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO=<(image_cache_capacity_multiplier_when_playing_video)',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/account/account.gyp:account',
        '<(DEPTH)/cobalt/audio/audio.gyp:audio',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/debug/debug.gyp:debug',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
        '<(DEPTH)/cobalt/h5vcc/h5vcc.gyp:h5vcc',
        '<(DEPTH)/cobalt/input/input.gyp:input',
        '<(DEPTH)/cobalt/layout/layout.gyp:layout',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/media/media.gyp:media',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
        '<(DEPTH)/cobalt/webdriver/webdriver.gyp:webdriver',
        '<(DEPTH)/cobalt/xhr/xhr.gyp:xhr',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        'browser_bindings.gyp:bindings',
        'screen_shot_writer',
      ],
      'conditions': [
        ['enable_about_scheme == 1', {
          'defines': [ 'ENABLE_ABOUT_SCHEME' ],
        }],
        ['OS=="starboard" or (OS=="lb_shell" and target_arch == "ps3")', {
          'dependencies': [
            '<(DEPTH)/nb/nb.gyp:nb',
          ],
        }],
      ],
    },

    {
      # This target provides functionality for creating screenshots of a
      # render tree and writing it to disk.
      'target_name': 'screen_shot_writer',
      'type': 'static_library',
      'variables': {
        # This target includes non-Cobalt code that produces pendantic
        # warnings as errors.
        'cobalt_code': 0,
      },
      'conditions': [
        ['enable_screenshot==1', {
          'sources': [
            'screen_shot_writer.h',
            'screen_shot_writer.cc',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/base/base.gyp:base',
            '<(DEPTH)/cobalt/renderer/rasterizer/skia/skia/skia.gyp:skia',
            '<(DEPTH)/cobalt/renderer/test/png_utils/png_utils.gyp:png_utils',
          ],
          'all_dependent_settings': {
            'defines': [ 'ENABLE_SCREENSHOT', ],
          },
        }],
      ],
    },

    {
      'target_name': 'browser_copy_test_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'browser_copy_test_data',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/browser/testdata/',
            ],
            'output_dir': 'cobalt/browser/testdata/',
          },
          'includes': [ '../build/copy_test_data.gypi' ],
        },
      ],
    },

    {
      'target_name': 'browser_copy_debug_console',
      'type': 'none',
      'actions': [
        {
          'action_name': 'browser_copy_debug_console',
          'variables': {
            'input_files': [
              '<(DEPTH)/cobalt/browser/debug_console/',
            ],
            'output_dir': 'cobalt/browser/debug_console/',
          },
          'includes': [ '../build/copy_web_data.gypi' ],
        },
      ],
    },
  ],
}

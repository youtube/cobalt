# Copyright 2014 Google Inc. All Rights Reserved.
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
# media/renderer interface.

{
  'variables': {
    'has_zzuf': '<!(python ../../../build/dir_exists.py ../../../third_party/zzuf)',
  },
  'targets': [
    # This target can choose the correct media dependency.
    {
      'target_name': 'media',
      'type': 'static_library',
      'conditions': [
        ['cobalt_media_source_2016==1', {
          'dependencies': [
            '<(DEPTH)/cobalt/media/media2.gyp:media2',
          ],
        }, {
          'dependencies': [
            '<(DEPTH)/cobalt/media/media.gyp:media',
          ],
        }],
      ],
    },

    # This target will build a sandbox application that allows for easy
    # experimentation with the media interface on any platform.  This can
    # also be useful for visually inspecting the output that the Cobalt
    # media stack is producing.
    {
      'target_name': 'web_media_player_sandbox',
      'type': '<(final_executable_type)',
      'sources': [
        'media_sandbox.cc',
        'media_sandbox.h',
        'web_media_player_helper.cc',
        'web_media_player_helper.h',
        'web_media_player_sandbox.cc',
      ],
      'dependencies': [
        'media',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        # Use test data from browser to avoid keeping two copies of video files.
        '<(DEPTH)/cobalt/browser/browser.gyp:browser_copy_test_data',
        '<(DEPTH)/cobalt/loader/loader.gyp:loader',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<@(cobalt_platform_dependencies)',
      ],
    },

    {
      'target_name': 'web_media_player_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'web_media_player_sandbox',
      ],
      'variables': {
        'executable_name': 'web_media_player_sandbox',
      },
      'includes': [ '../../../starboard/build/deploy.gypi' ],
    },
  ],
  'conditions': [
    ['sb_media_platform == "starboard" and cobalt_media_source_2016==1', {
      'targets': [
        {
          'target_name': 'media2_sandbox',
          'type': '<(final_executable_type)',
          'sources': [
            'media2_sandbox.cc',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/base/base.gyp:base',
            '<(DEPTH)/cobalt/math/math.gyp:math',
            '<(DEPTH)/cobalt/media/media2.gyp:media2',
          ],
        },
        {
          'target_name': 'media2_sandbox_deploy',
          'type': 'none',
          'dependencies': [
            'media2_sandbox',
          ],
          'variables': {
            'executable_name': 'media2_sandbox',
          },
          'includes': [ '../../../starboard/build/deploy.gypi' ],
        },
      ],
    }],
    ['OS == "starboard" and has_zzuf == "True" and cobalt_media_source_2016!=1', {
      'targets': [
        # This target will build a sandbox application that allows for fuzzing
        # decoder.
        {
          'target_name': 'raw_video_decoder_fuzzer',
          'type': '<(final_executable_type)',
          'sources': [
            'fuzzer_app.cc',
            'fuzzer_app.h',
            'media_sandbox.cc',
            'media_sandbox.h',
            'media_source_demuxer.cc',
            'media_source_demuxer.h',
            'raw_video_decoder_fuzzer.cc',
            'zzuf_fuzzer.cc',
            'zzuf_fuzzer.h',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/base/base.gyp:base',
            # Use test data from browser to avoid keeping two copies of video files.
            '<(DEPTH)/cobalt/browser/browser.gyp:browser_copy_test_data',
            '<(DEPTH)/cobalt/loader/loader.gyp:loader',
            '<(DEPTH)/cobalt/media/media.gyp:media',
            '<(DEPTH)/cobalt/network/network.gyp:network',
            '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
            '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
            '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
            '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
            '<(DEPTH)/third_party/zzuf/zzuf.gyp:zzuf',
          ],
        },

        {
          'target_name': 'raw_video_decoder_fuzzer_deploy',
          'type': 'none',
          'dependencies': [
            'raw_video_decoder_fuzzer',
          ],
          'variables': {
            'executable_name': 'raw_video_decoder_fuzzer',
          },
          'includes': [ '../../../starboard/build/deploy.gypi' ],
        },

        # This target will build a sandbox application that allows for fuzzing
        # shell demuxers.
        {
          'target_name': 'shell_demuxer_fuzzer',
          'type': '<(final_executable_type)',
          'sources': [
            'fuzzer_app.cc',
            'fuzzer_app.h',
            'in_memory_data_source.cc',
            'in_memory_data_source.h',
            'media_sandbox.cc',
            'media_sandbox.h',
            'shell_demuxer_fuzzer.cc',
            'zzuf_fuzzer.cc',
            'zzuf_fuzzer.h',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/base/base.gyp:base',
            # Use test data from browser to avoid keeping two copies of video files.
            '<(DEPTH)/cobalt/browser/browser.gyp:browser_copy_test_data',
            '<(DEPTH)/cobalt/loader/loader.gyp:loader',
            '<(DEPTH)/cobalt/media/media.gyp:media',
            '<(DEPTH)/cobalt/network/network.gyp:network',
            '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
            '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
            '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
            '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
            '<(DEPTH)/third_party/zzuf/zzuf.gyp:zzuf',
          ],
        },

        {
          'target_name': 'shell_demuxer_fuzzer_deploy',
          'type': 'none',
          'dependencies': [
            'shell_demuxer_fuzzer',
          ],
          'variables': {
            'executable_name': 'shell_demuxer_fuzzer',
          },
          'includes': [ '../../../starboard/build/deploy.gypi' ],
        },
      ],
    }],
  ],
}

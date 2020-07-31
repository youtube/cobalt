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

# This is a sample sandbox application for experimenting with the Cobalt
# media/renderer interface.

{
  'variables': {
    'has_zzuf': '<!pymod_do_main(starboard.build.gyp_functions dir_exists ../../../third_party/zzuf)',
  },
  'targets': [
    # This target will build a sandbox application that allows for easy
    # experimentation with the media interface on any platform.  This can
    # also be useful for visually inspecting the output that the Cobalt
    # media stack is producing.
    {
      'target_name': 'media_sandbox',
      'type': '<(final_executable_type)',
      'sources': [
        'media2_sandbox.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/media/media.gyp:media',
      ],
    },
    {
      'target_name': 'media_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'media_sandbox',
      ],
      'variables': {
        'executable_name': 'media_sandbox',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      'target_name': 'web_media_player_sandbox',
      'type': '<(final_executable_type)',
      'sources': [
        'format_guesstimator.cc',
        'format_guesstimator.h',
        'media_sandbox.cc',
        'media_sandbox.h',
        'web_media_player_helper.cc',
        'web_media_player_helper.h',
        'web_media_player_sandbox.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        # Use test data from demos to avoid keeping two copies of video files.
        '<(DEPTH)/cobalt/demos/demos.gyp:copy_demos',
        '<(DEPTH)/cobalt/loader/loader.gyp:loader',
        '<(DEPTH)/cobalt/media/media.gyp:media',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
        '<(DEPTH)/url/url.gyp:url',
      ],
      'conditions': [
        ['sb_evergreen == 0', {
          'dependencies': [
            '<@(cobalt_platform_dependencies)',
          ],
        }],
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
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}

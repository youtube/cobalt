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
# render tree/renderer interface.

{
  'targets': [
    {
      'target_name': 'renderer_sandbox',
      'type': '<(final_executable_type)',
      'sources': [
        'renderer_sandbox_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
        '<(DEPTH)/cobalt/renderer/test/scenes/scenes.gyp:scenes',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
      ],
    },

    # This target will build a sandbox application that allows for easy
    # experimentation with the renderer interface on any platform.  This can
    # also be useful for visually inspecting the output that the Cobalt
    # renderer is producing.
    {
      'target_name': 'renderer_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'renderer_sandbox',
      ],
      'variables': {
        'executable_name': 'renderer_sandbox',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

    {
      'target_name': 'scaling_text_sandbox',
      'type': '<(final_executable_type)',
      'sources': [
        'scaling_text_sandbox_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
        '<(DEPTH)/cobalt/renderer/test/scenes/scenes.gyp:scenes',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
      ],
    },

    # This target will build a sandbox application that allows for easy
    # experimentation with the renderer's handling of text where its scale
    # is constantly animating, which for many implementations can be a
    # performance problem.
    {
      'target_name': 'scaling_text_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'scaling_text_sandbox',
      ],
      'variables': {
        'executable_name': 'scaling_text_sandbox',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}

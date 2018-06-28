# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'input',
      'type': 'static_library',
      'sources': [
        'camera_3d.h',
        'create_default_camera_3d.h',
        'input_device_manager.h',
        'input_device_manager_desktop.cc',
        'input_device_manager_desktop.h',
        'input_device_manager_fuzzer.cc',
        'input_device_manager_fuzzer.h',
        'input_device_manager_starboard.cc',
        'input_poller.h',
        'input_poller_impl.cc',
        'input_poller_impl.h',
        'key_event_handler.cc',
        'key_event_handler.h',
        'key_repeat_filter.cc',
        'key_repeat_filter.h',
        'keypress_generator_filter.cc',
        'keypress_generator_filter.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
      ],
      'conditions': [
        ['enable_vr==1', {
          'sources': [
            'private/camera_3d_vr.cc',
            'private/camera_3d_vr.h',
          ],
        }, { # enable_vr!=1
          'sources': [
            'camera_3d_input_poller.cc',
            'camera_3d_input_poller.h',
          ],
        }],
      ],
    },
  ],
}

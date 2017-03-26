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
  'targets': [
    {
      'target_name': 'input',
      'type': 'static_library',
      'sources': [
        'input_device_manager.h',
        'input_device_manager_fuzzer.cc',
        'input_device_manager_fuzzer.h',
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
        ['OS=="starboard"', {
          'sources': [
            'input_device_manager_desktop.cc',
            'input_device_manager_desktop.h',
            'input_device_manager_starboard.cc',
          ],
        }],
        ['OS!="starboard" and actual_target_arch=="ps3"', {
          'sources': [
            'input_device_manager_ps3.cc',
            'input_device_manager_ps3.h',
            'keycode_conversion_ps3.cc',
            'keycode_conversion_ps3.h',
          ],
        }],
        ['OS!="starboard" and actual_target_arch=="win"', {
          'sources': [
            'input_device_manager_<(actual_target_arch).cc',
            'input_device_manager_desktop.cc',
            'input_device_manager_desktop.h',
          ],
        }],
      ],
    },
  ],
}

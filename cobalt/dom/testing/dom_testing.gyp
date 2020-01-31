# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
      'target_name': 'dom_testing',
      'type': 'static_library',
      'sources': [
        'gtest_workarounds.h',
        'html_collection_testing.h',
        'mock_event_listener.h',
        'mock_layout_boxes.h',
        'stub_css_parser.cc',
        'stub_css_parser.h',
        'stub_environment_settings.h',
        'stub_script_runner.cc',
        'stub_script_runner.h',
        'stub_window.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/browser/browser_bindings.gyp:bindings',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:cssom',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
      ],
    },
  ],
}

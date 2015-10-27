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

# This files contains all targets that should be created by gyp_cobalt by
# default.

{
  'targets': [
    {
      'target_name': 'Default',
      'default_project': 1,
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/cobalt/browser/browser.gyp:cobalt',
      ],
    },

    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_unittests',
        '<(DEPTH)/cobalt/audio/audio.gyp:*',
        '<(DEPTH)/cobalt/base/base.gyp:*',
        '<(DEPTH)/cobalt/bindings/testing/testing.gyp:*',
        '<(DEPTH)/cobalt/browser/browser.gyp:*',
        '<(DEPTH)/cobalt/csp/csp.gyp:*',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:*',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:*',
        '<(DEPTH)/cobalt/cssom/cssom_test.gyp:*',
        '<(DEPTH)/cobalt/debug/debug.gyp:*',
        '<(DEPTH)/cobalt/dom/dom.gyp:*',
        '<(DEPTH)/cobalt/dom/dom_test.gyp:*',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:*',
        '<(DEPTH)/cobalt/h5vcc/h5vcc.gyp:*',
        '<(DEPTH)/cobalt/input/input.gyp:*',
        '<(DEPTH)/cobalt/layout/layout.gyp:*',
        '<(DEPTH)/cobalt/layout_tests/layout_tests.gyp:*',
        '<(DEPTH)/cobalt/loader/loader.gyp:*',
        '<(DEPTH)/cobalt/math/math.gyp:*',
        '<(DEPTH)/cobalt/media/sandbox/sandbox.gyp:*',
        '<(DEPTH)/cobalt/network/network.gyp:*',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:*',
        '<(DEPTH)/cobalt/renderer/rasterizer_skia/skia/sandbox/sandbox.gyp:*',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:*',
        '<(DEPTH)/cobalt/renderer/sandbox/sandbox.gyp:*',
        '<(DEPTH)/cobalt/samples/samples.gyp:*',
        '<(DEPTH)/cobalt/script/javascriptcore.gyp:*',
        '<(DEPTH)/cobalt/storage/storage.gyp:*',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:*',
        '<(DEPTH)/cobalt/xhr/xhr.gyp:*',
      ],
      'conditions': [
        ['OS=="starboard"', {
          'dependencies': [
            '<(DEPTH)/starboard/starboard_all.gyp:starboard_all',
          ],
        }],
      ],
    },
  ],
}

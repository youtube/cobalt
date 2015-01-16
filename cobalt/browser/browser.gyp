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

{
  'targets': [

    {
      'target_name': 'cobalt',
      'type': '<(final_executable_type)',
      'sources': [
        'main.cc',
      ],
      'dependencies': [
        # TODO(***REMOVED***): This is only for Oxide, remove afterwards.
        '<(DEPTH)/cobalt/browser/browser_copy_test_data.gyp:browser_copy_test_data',
        'browser',
      ],
    },

    {
      'target_name': 'cobalt_deploy',
      'type': 'none',
      'dependencies': [
        'cobalt',
      ],
      'variables': {
        'executable_name': 'cobalt',
      },
      'includes': [ '../build/deploy.gypi' ],
    },

    {
      'target_name': 'browser',
      'type': 'static_library',
      'sources': [
        'application.cc',
        'application.h',
        'browser_module.cc',
        'browser_module.h',
        'dummy_render_tree_source.cc',
        'dummy_render_tree_source.h',
        'switches.cc',
        'switches.h',
        'web_content.cc',
        'web_content.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/<(actual_target_arch)/platform_browser.gyp:platform_browser',
        '<(DEPTH)/cobalt/browser/bindings/javascriptcore.gyp:bindings',
        '<(DEPTH)/cobalt/browser/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/browser/loader/loader.gyp:resource_loader',
        '<(DEPTH)/cobalt/browser/script/javascriptcore.gyp:engine',
        '<(DEPTH)/cobalt/layout/layout.gyp:layout',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
      ],
    },

    {
      'target_name': 'browser_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'web_content_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser_copy_test_data.gyp:browser_copy_test_data',
        '<(DEPTH)/cobalt/browser/testing/testing.gyp:browser_testing',
        'browser',
      ],
    },

    {
      'target_name': 'browser_test_deploy',
      'type': 'none',
      'dependencies': [
        'browser_test',
      ],
      'variables': {
        'executable_name': 'browser_test',
      },
      'includes': [ '../build/deploy.gypi' ],
    },

    # This target make the browser component tests visible to the toolchain.
    {
      'target_name': 'browser_component_tests',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/cobalt/browser/dom/dom.gyp:dom_test_deploy',
      ],
    },

  ],
}

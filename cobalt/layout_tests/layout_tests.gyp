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
    'sb_pedantic_warnings': 1,
  },

  # A library to support code that wishes to load a URL and take a snapshot
  # of the results.  "snapshot" in this case can mean a screenshot for
  # layout tests, or a performance profile for benchmarks, or anything else.
  # It uses the TestRunner interface to control when the snapshot is taken.
  'targets': [
    {
      # TODO: Consider renaming the entire module to something like
      #       "layout_snapshots" instead of "layout_tests", since the module is
      #       home to more than just layout_tests (e.g.  layout_benchmarks
      #       also).
      'target_name': 'layout_test_utils',
      'type': 'static_library',
      'sources': [
        'layout_snapshot.cc',
        'layout_snapshot.h',
        'test_parser.cc',
        'test_parser.h',
        'test_utils.cc',
        'test_utils.h',
        'web_platform_test_parser.cc',
        'web_platform_test_parser.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        'layout_copy_test_data',
      ],
    },

    {
      'target_name': 'layout_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          '<(DEPTH)/cobalt/layout_tests/testdata/',
        ],
        'content_test_output_subdir': 'cobalt/layout_tests',
      },
      'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
    },

    {
      'target_name': 'layout_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'layout_tests.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:render_tree_pixel_tester',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'layout_test_utils',
      ],
    },

    {
      'target_name': 'layout_tests_deploy',
      'type': 'none',
      'dependencies': [
        'layout_tests',
      ],
      'variables': {
        'executable_name': 'layout_tests',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

    {
      'target_name': 'layout_benchmarks',
      'type': '<(final_executable_type)',
      'sources': [
        'layout_benchmarks.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:run_all_benchmarks',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        'layout_test_utils',
      ],
    },
    {
      'target_name': 'layout_benchmarks_deploy',
      'type': 'none',
      'dependencies': [
        'layout_benchmarks',
      ],
      'variables': {
        'executable_name': 'layout_benchmarks',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
    {
      'target_name': 'web_platform_tests',
      'type': '<(gtest_target_type)',
      'sources': [
        'web_platform_tests.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/browser/browser.gyp:browser',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'layout_test_utils',
      ],
    },

    {
      'target_name': 'web_platform_tests_deploy',
      'type': 'none',
      'dependencies': [
        'web_platform_tests',
      ],
      'variables': {
        'executable_name': 'web_platform_tests',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

  ],
}

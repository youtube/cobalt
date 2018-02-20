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

# This is a sample GYP files that shows the common patterns that should be used
# to create GYP files in Cobalt.

{
  'targets': [
    # Independent components should be broken into separate and testable
    # libraries.
    {
      'target_name': 'simple_example_lib',
      'type': 'static_library',
      'sources': [
        'simple_example.cc',
        'simple_example.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
      ],
    },

    # This target is responsible for copying content required to run
    # simple_example to (PRODUCT_DIR). This target is optional and should
    # be added only if the executable has custom content.
    {
      'target_name': 'simple_example_content',
      'type': 'none',
      # Executable specific content logic should be added here.
      # ...
    },

    # An executable target is responsible for building the executable
    # and also making sure that all the necessary data needed to run the
    # executable from (PRODUCT_DIR) was copied over.
    {
      'target_name': 'simple_example',
      # Using (final_executable_type) instead of executable allows us
      # to compile the target into a library on platforms that package the
      # native code into a bundle (ex. Android).
      'type': '<(final_executable_type)',
      'variables': {
        # Override the default main thread stack size and set it to 1MB.
        'main_thread_stack_size': 1048576,
      },
      'sources': [
        'simple_example_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        'simple_example_content',
        'simple_example_lib',
      ],
    },

    # This target will build simple_example and deploy the required data
    # (executable + content) to a platform specific location needed to run
    # executable on the target platform.
    {
      'target_name': 'simple_example_deploy',
      'type': 'none',
      'dependencies': [
        'simple_example',
      ],
      'variables': {
        'executable_name': 'simple_example',
      },
      'includes': [ '../../../starboard/build/deploy.gypi' ],
    },

    # This target will create a test for simple_example.
    {
      'target_name': 'simple_example_test',
      # Using (gtest_target_type) instead of executable allows us to compile
      # the target into a library on platforms that package the native code
      # into a bundle (ex. Android).
      'type': '<(gtest_target_type)',
      'sources': [
        'simple_example_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/test/test.gyp:run_all_unittests',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'simple_example_lib',
        'simple_example_copy_test_data',
      ],
    },

    # This target is optional and is only needed if tests are using
    # external data.
    {
      'target_name': 'simple_example_copy_test_data',
      'type': 'none',
      'variables': {
        'content_test_input_files': [
          'testdata',
        ],
        'content_test_output_subdir': 'cobalt/samples',
      },
      'includes': [ '<(DEPTH)/starboard/build/copy_test_data.gypi' ],
    },

    # This target will deploy the test and its data to a platform specific
    # location needed to run the test on the target platform.
    {
      'target_name': 'simple_example_test_deploy',
      'type': 'none',
      'dependencies': [
        'simple_example_test',
      ],
      'variables': {
        'executable_name': 'simple_example_test',
      },
      'includes': [ '../../../starboard/build/deploy.gypi' ],
    },
  ],
}

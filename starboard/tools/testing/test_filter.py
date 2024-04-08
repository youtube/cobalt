#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Class used to filter out unit tests."""

# Constant to use for filtering all unit tests from a specific test executable.
# Provide it as the "test_name" argument to TestFilter's constructor.
FILTER_ALL = 'FILTER_ALL'
DISABLE_TESTING = 'DISABLE_TESTING'

# Tests added for Evergreen only
EVERGREEN_TESTS = [
    'app_key_test', 'app_key_files_test', 'drain_file_test',
    'slot_management_test'
]

# Tests that can only be run on Evergreen compatible host platforms, as set
# by sb_is_evergreen_compatible GN config flag.
EVERGREEN_COMPATIBLE_TESTS = [
    # TODO(b/292138589): Fails on various linux configs.
    # 'nplb_evergreen_compat_tests',
    'elf_loader_test',
    'installation_manager_test',
    'reset_evergreen_update_test',
]


class TestFilter(object):
  """Container for data used to filter out a unit test.

  Initialization is done with three arguments:

  target_name:  The name of the unit test binary from which to remove tests.
  test_name:  The name of a specific test from the provided test target, or a
    constant from this module defining a group of tests to filter.
  """

  def __init__(self, target_name, test_name):
    self.target_name = target_name
    self.test_name = test_name

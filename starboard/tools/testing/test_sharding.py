#!/usr/bin/python
#
# Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
"""
Provides utilities for reading the sharding configuration from JSON and parsing
it into the corresponding allocation of tests to shards.
"""

import os
import json
import logging

_SHARDING_CONFIG_FILE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'sharding_configuration.json')


class ShardingTestConfig(object):
  """
  Class that encapsulates the sharding configuration for a given platform.
  """

  RUN_FULL_TEST = 'run_full_test'
  RUN_PARTIAL_TEST = 'run_partial_test'
  SKIP_TEST = 'skip_test'

  def __init__(self, platform, test_targets, platform_sharding_config=None):
    try:
      self.platform_sharding_config = platform_sharding_config
      if not platform_sharding_config:
        self.platform_sharding_config = self._read_platform_sharding_config(
            platform)
      # Create the default shard, and add it to the config.
      default_shard = self._create_default_shard(self.platform_sharding_config,
                                                 test_targets)
      self.platform_sharding_config.insert(0, default_shard)
      logging.debug(
          '%s',
          json.dumps(self.platform_sharding_config, sort_keys=True, indent=2))
    except FileNotFoundError:
      raise RuntimeError('No sharding configuration file found.')

  def _read_platform_sharding_config(self,
                                     platform,
                                     filename=_SHARDING_CONFIG_FILE):
    with open(filename, 'r') as file:
      sharding_json = json.load(file)
      for platform_key in sharding_json:
        if platform in platform_key:
          return sharding_json[platform_key]
      return sharding_json['default']

  def _find_unmapped_test_chunks(self, platform_sharding_config):
    """
    This function determines which test chunks have not been assigned by
    enumerating all possible test chunks, given the existing incomplete sharding
    configuration.

    E.g. if the sharding configuration consists of:
    config  = [
      {
        "foo_test": [2, 3],
      },
      {
        "foo_test": [1, 3],
      }
    ]

    Then we can assume that the [3, 3] chunk is unmapped.
    """
    # Iterate over all chunks:
    # - Create a mapping between test_name and chunk_total
    # - Record the unmapped chunk_index values for each test_name
    chunk_totals_by_test = {}
    unmapped_chunk_index_by_test = {}
    for shard_chunk in platform_sharding_config:
      for test_name, chunk_info in shard_chunk.items():
        # The entire test is represented by '*' in the |chunk_info| field.
        if not isinstance(chunk_info, list):
          unmapped_chunk_index_by_test[test_name] = set()
          continue
        # The test chunk is represented by [index, count], where:
        # count >= index >= 1
        chunk_index, chunk_total = chunk_info
        if test_name not in chunk_totals_by_test:
          chunk_totals_by_test[test_name] = chunk_total
          unmapped_chunk_index_by_test[test_name] = set(
              i for i in range(1, chunk_total + 1))
        unmapped_chunk_index_by_test[test_name].remove(chunk_index)

    logging.debug('Printing all tests with unmapped chunks:')
    for k, v in unmapped_chunk_index_by_test.items():
      logging.debug('%s: %s', k, v)
    logging.debug('Printing all tests with corresponding chunk sizes:')
    logging.debug(json.dumps(chunk_totals_by_test, sort_keys=True, indent=2))

    return unmapped_chunk_index_by_test, chunk_totals_by_test

  def _create_default_shard(self, platform_sharding_config, test_targets):
    """
    The default shard consists of all unmapped test chunks, and unmapped tests.

    This function generates the default shard using the list of all test targets
    and the currently configured set of test chunks.

    A test chunk is defined as a subset of test cases from the test suite, which
    corresponds to the tests run by GTEST when provided the following arguments:

    $ ./foo_test --gtest_shard_index=INDEX --gtest_total_shards=COUNT

    Where COUNT > INDEX > 1.
    """
    unmapped_chunk_index_by_test, chunk_totals_by_test = (
        self._find_unmapped_test_chunks(platform_sharding_config))

    # Add the entire test to the default shard, if the test is not found in the
    # list of unmapped chunks.
    unlisted_tests = set(test_targets) - set(
        unmapped_chunk_index_by_test.keys())
    default_shard = {name: '*' for name in unlisted_tests}
    # Add the unmapped test chunks.
    for test_name, unmapped_chunks in unmapped_chunk_index_by_test.items():
      if len(unmapped_chunks) == 0:
        # Test has no unmapped chunks.
        continue
      # Shard cannot contain multiple chunks from the same test.
      if len(unmapped_chunks) > 1:
        raise ValueError('Invalid Sharding Configuration: default shard must '
                         'not contain multiple chunks from the same test ('
                         'test:{} unmapped_chunks:{}).'.format(
                             test_name, unmapped_chunks))
      # Add the unmapped test chunk to the default shard.
      chunk_index = list(unmapped_chunks)[0]
      chunk_total = chunk_totals_by_test[test_name]
      default_shard[test_name] = [chunk_index, chunk_total]

    return default_shard

  def get_test_run_config(self, test_target, shard_index):
    """
    Returns whether the input test is run in the input shard (specified by its
    index). If the test is run as part of the shard, then it also provides the
    additional parameters to subdivide the test (if needed). Otherwise the test
    is assumed to be fully run in the shard.
    """
    shard_test_targets = self.platform_sharding_config[shard_index]
    if test_target in shard_test_targets:
      test_run_config = shard_test_targets[test_target]
      if test_run_config == '*':
        return (self.RUN_FULL_TEST, 0, 0)
      else:
        sub_shard_index = test_run_config[0] - 1
        sub_shard_count = test_run_config[1]
        return (self.RUN_PARTIAL_TEST, sub_shard_index, sub_shard_count)
    else:
      return (self.SKIP_TEST, 0, 0)

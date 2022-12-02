#!/usr/bin/env python3

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
#
"""Tests the |test_sharding| module."""

import unittest

from starboard.tools.testing import test_sharding


class ValidateTestShardingConfigTests(unittest.TestCase):
  """
  Test harness for validating the generation of the default shard's test
  allocation when loading shard configuration.
  """

  TEST_TARGETS = ['foo', 'bar', 'baz']

  def verify_test_run_config(self, test_target, got_chunk,
                             expected_default_shard):
    test_action, chunk_index, chunk_total = got_chunk
    # Chunk Index is 1-indexed when parsing config.
    chunk_index += 1
    # Test is skipped
    if test_action == test_sharding.ShardingTestConfig.SKIP_TEST:
      self.assertNotIn(
          test_target, expected_default_shard,
          'Test {} is not skipped in default shard.'.format(test_target))
    # Test is fully run
    if test_action == test_sharding.ShardingTestConfig.RUN_FULL_TEST:
      self.assertIn(
          test_target, expected_default_shard,
          'Test {} is not present in default shard'.format(test_target))
      self.assertEqual(
          expected_default_shard[test_target], '*',
          'Test {} is not fully run in default shard. Instead is '
          'run partially ({} of {} chunks)'.format(test_target, chunk_index,
                                                   chunk_total))
    # Test is partially run
    if test_action == test_sharding.ShardingTestConfig.RUN_PARTIAL_TEST:
      self.assertIn(
          test_target, expected_default_shard,
          'Test {} is not present in default shard'.format(test_target))
      self.assertEqual(expected_default_shard[test_target][0], chunk_index,
                       'Test {} has incorrect chunk index'.format(test_target))
      self.assertEqual(expected_default_shard[test_target][1], chunk_total,
                       'Test {} has incorrect chunk total'.format(test_target))

  def load_sharding_config(self, input_shards, expected_default_shard):
    sharding_config = test_sharding.ShardingTestConfig(
        'default', self.TEST_TARGETS, platform_sharding_config=input_shards)
    for test_target in self.TEST_TARGETS:
      got_chunk = sharding_config.get_test_run_config(test_target, 0)
      self.verify_test_run_config(test_target, got_chunk,
                                  expected_default_shard)

  def testNoTestsInShardWhole(self):
    input_shards = [{'foo': '*'}, {'bar': '*'}, {'baz': '*'}]
    expected_default_shard = {}
    self.load_sharding_config(input_shards, expected_default_shard)

  def testNoTestsInShardChunked(self):
    input_shards = [{
        'foo': [1, 2],
        'baz': [2, 2]
    }, {
        'bar': [1, 2],
        'foo': [2, 2]
    }, {
        'baz': [1, 2],
        'bar': [2, 2]
    }]
    expected_default_shard = {}
    self.load_sharding_config(input_shards, expected_default_shard)

  def testWholeTestsInShard(self):
    input_shards = [{'foo': '*'}]
    expected_default_shard = {'bar': '*', 'baz': '*'}
    self.load_sharding_config(input_shards, expected_default_shard)

  def testPartialTestsInShard(self):
    # Output shard contains a test split into 2 chunks.
    input_shards = [{'foo': '*'}, {'baz': [2, 2]}, {'bar': '*'}]
    expected_default_shard = {'baz': [1, 2]}
    self.load_sharding_config(input_shards, expected_default_shard)

    # Output shard contains a test split into 3 chunks.
    input_shards = [{'foo': [3, 3]}, {'foo': [1, 3]}, {'bar': '*', 'baz': '*'}]
    expected_default_shard = {'foo': [2, 3]}
    self.load_sharding_config(input_shards, expected_default_shard)

  def testWholeAndPartialTestsInShard(self):
    # Output shard contains a test split into 2 chunks.
    input_shards = [{'foo': '*'}, {'bar': [2, 2]}]
    expected_default_shard = {'bar': [1, 2], 'baz': '*'}
    self.load_sharding_config(input_shards, expected_default_shard)

    # Output shard contains a test split into 3 chunks.
    input_shards = [{'bar': [1, 3]}, {'bar': [2, 3]}]
    expected_default_shard = {'bar': [3, 3], 'foo': '*', 'baz': '*'}
    self.load_sharding_config(input_shards, expected_default_shard)

  def testManyPartialTestsInShard(self):
    input_shards = [{
        'bar': [1, 3]
    }, {
        'bar': [2, 3]
    }, {
        'baz': [1, 2],
        'foo': [2, 2]
    }]
    expected_default_shard = {'bar': [3, 3], 'foo': [1, 2], 'baz': [2, 2]}
    self.load_sharding_config(input_shards, expected_default_shard)

  def testInvalidPossibleConfig(self):
    input_shards = [{'foo': [1, 3]}]
    expected_default_shard = {}  # No valid output shard is expected.
    self.assertRaises(ValueError, self.load_sharding_config, input_shards,
                      expected_default_shard)


if __name__ == '__main__':
  unittest.main()

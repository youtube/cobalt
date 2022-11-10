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

SHARDING_CONFIG_FILE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'sharding_configuration.json')


class ShardingTestConfig(object):
  """
  Class that encapsulates the sharding configuration for a given platform.
  """

  RUN_FULL_TEST = 'run_full_test'
  RUN_PARTIAL_TEST = 'run_partial_test'
  SKIP_TEST = 'skip_test'

  def __init__(self, platform):
    try:
      with open(SHARDING_CONFIG_FILE, 'r') as file:
        sharding_json = json.load(file)
      # Load this config by default.
      self.platform_sharding_config = sharding_json['default']
      # Check for specific platform if specified:
      for platform_key in sharding_json:
        if platform in platform_key:
          self.platform_sharding_config = sharding_json[platform_key]
    except FileNotFoundError:
      raise RuntimeError('No sharding configuration file found.')

  def GetTestRunConfig(self, test_target, shard_index):
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

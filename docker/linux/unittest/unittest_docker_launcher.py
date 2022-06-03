#!/usr/bin/env python
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
"""Entrypoint for running unit-tests shards in docker"""

import json
import logging
import os
import subprocess
import sys
import time

log = logging.getLogger(__name__)


def exc(cmd, check=False):
  log.info(cmd)
  try:
    output = subprocess.check_output(cmd, shell=True)
    log.info(output)
  except subprocess.CalledProcessError:
    if check:
      raise


def main(argv):
  shard_index = None
  if len(argv) >= 2:
    shard_index = int(argv[1])

  out_dir = '/out'

  exc('unzip -q {}/app_launcher -d /app_launcher_out'.format(out_dir))

  xvfb_prefix = ('xvfb-run -a --server-args="-screen 0 1920x1080x24'
                 '+render +extension GLX -noreset"')
  env_platform = os.getenv('PLATFORM')
  env_config = os.getenv('CONFIG')
  test_command = [
      'python', '/app_launcher_out/starboard/tools/testing/test_runner.py',
      '--run', '-o', out_dir, '-p', env_platform, '-c', env_config, '-l'
  ]

  if shard_index is not None:
    test_command.append('-s')
    test_command.append(str(shard_index))

  test_command = ' '.join(test_command)

  start_t = time.time()
  exc('{} {}'.format(xvfb_prefix, test_command))
  end_t = time.time()

  # Output shard timing information to file.
  duration_t = (end_t - start_t) / 60.0
  shard_timing_json = '{}/shard_timing.json'.format(out_dir)
  try:
    with open(shard_timing_json, 'r') as f:
      json_content = json.loads(f.read())
  except FileNotFoundError:
    json_content = {}
  json_content[str(shard_index)] = '{:.2f}'.format(duration_t)
  with open(shard_timing_json, 'w+') as f:
    json.dump(json_content, f, sort_keys=True, indent=2)


if __name__ == '__main__':
  logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
  main(sys.argv)

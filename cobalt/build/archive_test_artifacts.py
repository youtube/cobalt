#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Creates test artifacts tar with runtime dependencies."""

import argparse
import json
import os
import subprocess
import tempfile
from typing import List


def _make_tar(archive_path: str, file_list: str):
  """Creates the tar file. Uses tar command instead of tarfile for performance.
  """
  print(f'Creating {os.path.basename(archive_path)}')
  # Create temporary file to hold file list to not blow out the commandline.
  with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8') as temp_file:
    temp_file.write('\n'.join(sorted(file_list)))
    temp_file.flush()
    tar_cmd = ['tar', '-I gzip -1', '-cvf', archive_path, '-T', temp_file.name]
    subprocess.check_call(tar_cmd)


def create_archive(targets: List[str], source_dir: str, destination_dir: str,
                   platform: str, combine: bool):
  """Main logic. Collects runtime dependencies from the source directory for
  each target."""
  # TODO(b/382508397): Remove when dynamically generated.
  # Put the test targets in a json file in the archive.
  test_target_names = [target.split(':')[1] for target in targets]
  test_targets_json = os.path.join(source_dir, 'test_targets.json')
  with open(test_targets_json, 'w', encoding='utf-8') as test_targets_file:
    test_targets_file.write(
        json.dumps({
            'test_targets':
                test_target_names,
            'executables': [
                os.path.join(source_dir, target_name)
                for target_name in test_target_names
            ]
        }))

  deps = set([test_targets_json])
  for target in targets:
    target_path, target_name = target.split(':')
    # These paths are configured in test.gni:
    # https://github.com/youtube/cobalt/blob/main/testing/test.gni
    if platform.startswith('android'):
      deps_file = os.path.join(
          source_dir, 'gen.runtime', target_path,
          f'{target_name}__test_runner_script.runtime_deps')
    else:
      deps_file = os.path.join(source_dir, f'{target_name}.runtime_deps')

    with open(deps_file, 'r', encoding='utf-8') as runtime_deps_file:
      target_deps = {
          os.path.relpath(os.path.join(source_dir, line.strip()))
          for line in runtime_deps_file
      }
      deps |= target_deps

    if not combine:
      output_path = os.path.join(destination_dir, f'{target_name}_deps.tar.gz')
      _make_tar(output_path, deps)

  if combine:
    output_path = os.path.join(destination_dir, 'test_artifacts.tar.gz')
    _make_tar(output_path, deps)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-s',
      '--source-dir',
      required=True,
      help='The path to where the artifacts are stored. '
      'Typically the out folder.')
  parser.add_argument(
      '-d',
      '--destination-dir',
      required=True,
      help='The output directory. For linux test_artifacts.tar.gz is created '
      'with all test artifacts, else a `<target_name>_runtime_deps.tar.gz` is '
      'created for each target passed.')
  parser.add_argument(
      '-p', '--platform', required=True, help='The platform getting packaged.')
  parser.add_argument(
      '-t',
      '--targets',
      required=True,
      type=lambda arg: arg.split(','),
      help='The targets to package, comma-separated. Must be fully qualified '
      'for android.')
  args = parser.parse_args()

  create_archive(args.targets, args.source_dir, args.destination_dir,
                 args.platform, args.platform.startswith('linux'))

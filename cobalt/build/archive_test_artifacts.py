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
import shutil
import subprocess
import tempfile
from typing import List, Optional

# Exclude paths that contain files we don't need to run tests.
_EXCLUDE_DIRS = [
    '../../third_party/jdk/', 'lib.java/', './exe.unstripped/',
    './lib.unstripped/'
]


def _make_tar(archive_path: str,
              file_list: str,
              base_path: Optional[str] = None):
  """Creates the tar file. Uses tar command instead of tarfile for performance.
  """
  print(f'Creating {os.path.basename(archive_path)}')
  # Create temporary file to hold file list to not blow out the commandline.
  with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8') as temp_file:
    temp_file.write('\n'.join(sorted(file_list)))
    temp_file.flush()
    base_path_arg = ['-C', base_path] if base_path else []
    tar_cmd = [
        'tar', '-I gzip -1', '-cvf', archive_path, *base_path_arg, '-T',
        temp_file.name
    ]
    subprocess.check_call(tar_cmd)


def create_archive(targets: List[str], source_dir: str, destination_dir: str,
                   platform: str, uber_archive: bool):
  """Main logic. Collects runtime dependencies from the source directory for
  each target."""
  tar_root = '.' if platform.startswith('android') else source_dir
  # TODO(b/382508397): Remove when dynamically generated.
  # Put the test targets in a json file in the archive.
  test_target_names = [target.split(':')[1] for target in targets]
  test_targets_json = os.path.join(tar_root, 'test_targets.json')
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
      # Android assumes files will not be extracted to out dir.
      target_deps = {
          os.path.relpath(os.path.join(tar_root, line.strip()))
          for line in runtime_deps_file
          if not any(line.startswith(path) for path in _EXCLUDE_DIRS)
      }
      deps |= target_deps

    if not uber_archive:
      output_path = os.path.join(destination_dir, f'{target_name}_deps.tar.gz')
      base_path = source_dir if platform.startswith('android') else None
      _make_tar(output_path, deps, base_path)
      deps = set([test_targets_json])

  if uber_archive:
    output_path = os.path.join(destination_dir, 'test_artifacts.tar.gz')
    _make_tar(output_path, deps)


def copy_apks(targets: List[str], source_dir: str, destination_dir: str):
  """Copies the target APKs from the source directory to the destination.
  The path to the APK in the source directory (assumed here to be the out
  directory) is defined in build/config/android/rules.gni
  """
  for target in targets:
    _, target_name = target.split(':')
    apk_path = f'{source_dir}/{target_name}_apk/{target_name}-debug.apk'
    shutil.copy2(apk_path, destination_dir)


def main():
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
      help='The targets to package, comma-separated. Must be fully qualified, '
      'e.g. path/to:target.')
  args = parser.parse_args()

  uber_archive = args.platform.startswith('linux')
  create_archive(args.targets, args.source_dir, args.destination_dir,
                 args.platform, uber_archive)

  if args.platform.startswith('android'):
    copy_apks(args.targets, args.source_dir, args.destination_dir)


if __name__ == '__main__':
  main()

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
import os
import subprocess
import tempfile
from typing import List, Tuple

# Path prefixes that contain files we don't need to run tests.
_EXCLUDE_DIRS = [
    'obj/',
    'lib.java/',
    './exe.unstripped/',
    './lib.unstripped/',
    '../../third_party/jdk/',
]


def _make_tar(archive_path: str, file_lists: List[Tuple[str, str]]):
  """Creates the tar file. Uses tar command instead of tarfile for performance.
  """
  tar_cmd = ['tar', '-I', 'gzip -1', '-cvf', archive_path]
  tmp_files = []
  for file_list, base_dir in file_lists:
    if not file_list:
      continue
    # Create temporary file to hold file list to not blow out the commandline.
    # It will get cleaned up via implicit close.
    # pylint: disable=consider-using-with
    tmp_file = tempfile.NamedTemporaryFile(mode='w', encoding='utf-8')
    tmp_files.append(tmp_file)
    tmp_file.write('\n'.join(sorted(file_list)))
    tmp_file.flush()
    # Change the tar working directory and add the file list.
    tar_cmd += ['-C', base_dir, '-T', tmp_file.name]

  print(f'Running `{" ".join(tar_cmd)}`')  # pylint: disable=inconsistent-quotes
  subprocess.check_call(tar_cmd)

  archive_size = f'{os.path.getsize(archive_path) / 1024 / 1024:.2f} MB'
  print(f'Created {os.path.basename(archive_path)} ({archive_size})')


def create_archive(
    *,
    targets: List[str],
    source_dir: str,
    out_dir: str,
    destination_dir: str,
    archive_per_target: bool,
    use_android_deps_path: bool,
    flatten_deps: bool,
):
  """Main logic. Collects runtime dependencies for each target."""
  combined_deps = set()
  for target in targets:
    target_path, target_name = target.split(':')
    # Paths are configured in test.gni:
    # https://github.com/youtube/cobalt/blob/main/testing/test.gni
    if use_android_deps_path:
      deps_file = os.path.join(
          out_dir, 'gen.runtime', target_path,
          f'{target_name}__test_runner_script.runtime_deps')
    else:
      deps_file = os.path.join(out_dir, f'{target_name}.runtime_deps')
      if not os.path.exists(deps_file):
        # If |deps_file| doesn't exist it could be due to being generated with
        # the starboard_toolchain. In that case, we should look in subfolders.
        # For the time being, just try with an extra starboard/ in the path.
        deps_file = os.path.join(out_dir, 'starboard',
                                 f'{target_name}.runtime_deps')

    print('Collecting runtime dependencies for', target)
    with open(deps_file, 'r', encoding='utf-8') as runtime_deps_file:
      # The paths in the runtime_deps files are relative to the out folder.
      # Android tests expects files both in the out and source root folders
      # to be working directory archive whereas Linux tests expect it relative
      # to the binary.
      tar_root = '.' if flatten_deps else out_dir
      target_deps = set()
      target_src_root_deps = set()

      # Add test_targets.json to archive so that test runners know what to run.
      test_targets_json = os.path.join(out_dir, 'test_targets.json')
      if os.path.exists(test_targets_json):
        target_deps.add(os.path.join(tar_root, 'test_targets.json'))

      for line in runtime_deps_file:
        if any(line.startswith(path) for path in _EXCLUDE_DIRS):
          continue

        if flatten_deps and line.startswith('../../'):
          target_src_root_deps.add(line[6:])
        else:
          # Rebase all files to be relative to their respective root (source or
          # out dir) to be able to flatten them below. Chromium test runners
          # have access to the source directory in '../..' which ours (ODTs
          # especially) do not.
          rel_path = os.path.relpath(os.path.join(tar_root, line.strip()))
          target_deps.add(rel_path)
      combined_deps |= target_deps

      if archive_per_target:
        output_path = os.path.join(destination_dir,
                                   f'{target_name}_deps.tar.gz')
        if flatten_deps:
          _make_tar(output_path, [(target_deps, out_dir),
                                  (target_src_root_deps, source_dir)])
        else:
          _make_tar(output_path, [(target_deps, source_dir)])
  # Linux tests and deps are all bundled into a single tar file.
  if not archive_per_target:
    output_path = os.path.join(destination_dir, 'test_artifacts.tar.gz')
    _make_tar(output_path, [(combined_deps, source_dir)])


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-s', '--source-dir', required=True, help='The path to the source root.')
  parser.add_argument(
      '-o',
      '--out-dir',
      required=True,
      help='The path to where the build artifacts are stored.')
  parser.add_argument(
      '-d',
      '--destination-dir',
      required=True,
      help='The output directory. For linux test_artifacts.tar.gz is created '
      'with all test artifacts, else a `<target_name>_deps.tar.gz` is '
      'created for each target passed.')
  parser.add_argument(
      '-t',
      '--targets',
      required=True,
      type=lambda arg: arg.split(','),
      help='The targets to package, comma-separated. Must be fully qualified, '
      'e.g. path/to:target_name,other/path/to:target_name.')
  parser.add_argument(
      '--archive-per-target',
      action='store_true',
      help='Create a separate .tar.gz archive for each build target.')
  parser.add_argument(
      '--use-android-deps-path',
      action='store_true',
      help='Look for .runtime_deps files in the Android-specific path.')
  parser.add_argument(
      '--flatten-deps',
      action='store_true',
      help='Pass this argument to archive files from the source and out '
      'directories both at the root of the deps archive.')
  args = parser.parse_args()

  create_archive(
      targets=args.targets,
      source_dir=args.source_dir,
      out_dir=args.out_dir,
      destination_dir=args.destination_dir,
      archive_per_target=args.archive_per_target,
      use_android_deps_path=args.use_android_deps_path,
      flatten_deps=args.flatten_deps)


if __name__ == '__main__':
  main()

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
import logging
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import List, Tuple

# Path prefixes that contain files we don't need to run tests.
_EXCLUDE_DIRS = [
    'obj/',
    'lib.java/',
    './exe.unstripped/',
    './lib.unstripped/',
    '../../third_party/jdk/',
    'testing/buildbot/',
]

_ESSENTIAL_BROWSERTEST_DIRS = [
    'build/android',
    'build/util',
    'build/skia_gold_common',
    'testing',
    'third_party/android_build_tools',
    'third_party/catapult/common',
    'third_party/catapult/dependency_manager',
    'third_party/catapult/devil',
    'third_party/catapult/third_party',
    'third_party/logdog',
    'third_party/android_sdk/public/platform-tools',
    'third_party/android_platform/development/scripts',
    'third_party/llvm-build/Release+Asserts/bin',
    'tools/python',
]


def get_test_runner(build_dir, is_android):
  """Determines the relative path to the test runner."""
  script_rel = os.path.join('bin', 'run_cobalt_browsertests')
  if (is_android or os.path.isfile(os.path.join(build_dir, script_rel))):
    return script_rel

  return 'cobalt_browsertests'


def generate_runner_py(dst_path, target_map, template_path):
  """Generates the run_tests.py script inside the archive."""
  logging.info('Generating portable runner with targets: %s',
               ', '.join(target_map.keys()))
  with open(template_path, 'r', encoding='utf-8') as f:
    content = f.read()

    # Inject the target map into the template.
    target_map_repr = repr(target_map)

    if 'TARGET_MAP = {}' not in content:
      raise RuntimeError(
          f'Could not find TARGET_MAP placeholder in {template_path}')

    content = content.replace('TARGET_MAP = {}',
                              f'TARGET_MAP = {target_map_repr}')

    # Adjust the template to expect files at root instead of src/
    content = content.replace('src_dir = os.path.join(script_dir, "src")',
                              'src_dir = script_dir')

  with open(dst_path, 'w', encoding='utf-8') as f:
    f.write(content)
  os.chmod(dst_path, 0o755)


def _make_tar(archive_path: str, compression: str, compression_level: int,
              file_lists: List[Tuple[List[str], str]]):
  """Creates the tar file. Uses tar command instead of tarfile for performance.
  """
  archive_path = os.path.abspath(archive_path)
  if compression == 'gz':
    compression_program = f'gzip -{compression_level}'
  elif compression == 'xz':
    compression_program = f'xz -T0 -{compression_level}'
  elif compression == 'zstd':
    compression_program = f'zstd -T0 -{compression_level}'
  else:
    raise ValueError(f'Unsupported compression: {compression}')

  tar_cmd = [
      'tar', '--use-compress-program', compression_program, '-cvf', archive_path
  ]
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

  logging.info('Running `%s`', ' '.join(tar_cmd))
  subprocess.check_call(tar_cmd)

  archive_size = f'{os.path.getsize(archive_path) / 1024 / 1024:.2f} MB'
  logging.info('Created %s (%s)', os.path.basename(archive_path), archive_size)


def create_archive(
    *,
    targets: List[str],
    source_dir: str,
    out_dir: str,
    destination_dir: str,
    archive_per_target: bool,
    use_android_deps_path: bool,
    compression: str,
    compression_level: int,
    flatten_deps: bool,
    include_browsertests_runner: bool = False,
    include_depot_tools: bool = False,
):
  """Main logic. Collects runtime dependencies for each target."""
  source_dir = os.path.abspath(source_dir)
  out_dir = os.path.abspath(out_dir)
  destination_dir = os.path.abspath(destination_dir)
  os.makedirs(destination_dir, exist_ok=True)
  combined_deps = set()
  target_map = {}
  file_lists = []

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
        # Fallback 1: try common Android script-based test naming
        script_deps = os.path.join(
            out_dir, 'gen.runtime', target_path,
            f'{target_name}__test_runner_script.runtime_deps')
        if os.path.exists(script_deps):
          deps_file = script_deps
        else:
          # Fallback 2: try with starboard/ in path
          deps_file = os.path.join(out_dir, 'starboard',
                                   f'{target_name}.runtime_deps')

    logging.info('Collecting runtime dependencies for %s', target)
    if not os.path.exists(deps_file):
      logging.warning('Could not find runtime_deps file: %s', deps_file)
      continue

    # Add the deps file itself to the archive
    deps_file_rel = os.path.relpath(deps_file, source_dir)
    target_deps = set()
    target_src_root_deps = set()
    if flatten_deps:
      target_deps.add(os.path.relpath(deps_file, out_dir))
    else:
      combined_deps.add(deps_file_rel)

    # Determine runner info for target map
    build_rel_path = os.path.relpath(out_dir, source_dir)
    is_android = 'android' in build_rel_path.lower(
    ) or 'android' in out_dir.lower()
    test_runner_rel = get_test_runner(out_dir, is_android)
    test_runner_full = os.path.join(out_dir, test_runner_rel)
    test_runner_rel_to_src = os.path.relpath(test_runner_full, source_dir)
    target_map[target_name] = {
        'deps': deps_file_rel,
        'runner': test_runner_rel_to_src,
        'is_android': is_android,
        'build_dir': build_rel_path
    }

    if not archive_per_target and not flatten_deps:
      combined_deps.add(test_runner_rel_to_src)

    with open(deps_file, 'r', encoding='utf-8') as runtime_deps_file:
      # The paths in the runtime_deps files are relative to the out folder.
      # Android tests expects files both in the out and source root folders
      # to be working directory archive whereas Linux tests expect it relative
      # to the binary.
      tar_root = out_dir

      # Add test_targets.json to archive so that test runners know what to run.
      test_targets_json = os.path.join(out_dir, 'test_targets.json')
      if os.path.exists(test_targets_json):
        if flatten_deps:
          target_deps.add('test_targets.json')
        else:
          target_deps.add(os.path.relpath(test_targets_json, source_dir))

      for line in runtime_deps_file:
        line = line.strip()
        if any(line.startswith(path) for path in _EXCLUDE_DIRS):
          continue

        if flatten_deps:
          if line.startswith('../../'):
            target_src_root_deps.add(line[6:])
          else:
            target_deps.add(line)
        else:
          # Rebase all files to be relative to the source root
          full_path = os.path.abspath(os.path.join(tar_root, line))
          rel_path = os.path.relpath(full_path, source_dir)
          target_deps.add(rel_path)

      if not archive_per_target and not flatten_deps:
        combined_deps |= target_deps

      if archive_per_target:
        output_path = os.path.join(destination_dir,
                                   f'{target_name}_deps.tar.{compression}')
        if flatten_deps:
          _make_tar(
              output_path,
              compression,
              compression_level,
              [(list(target_deps), out_dir),
               (list(target_src_root_deps), source_dir)],
          )
        else:
          raise ValueError('Unsupported configuration.')
  # Linux tests and deps are all bundled into a single tar file.
  if not archive_per_target:
    output_path = os.path.join(destination_dir,
                               f'test_artifacts.tar.{compression}')

    if include_browsertests_runner:
      logging.info('Including browsertest runner artifacts')
      # Add essential directories
      for d in _ESSENTIAL_BROWSERTEST_DIRS:
        full_d = os.path.join(source_dir, d)
        if os.path.isdir(full_d):
          rel_d = os.path.relpath(full_d, source_dir)
          combined_deps.add(rel_d)

      # Add top-level build scripts
      for f in Path(source_dir).joinpath('build').glob('*.py'):
        rel_f = os.path.relpath(str(f), source_dir)
        combined_deps.add(rel_f)

      # Add vpython3 and run_browser_tests.py
      for f in [
          '.vpython3', 'v8/.vpython3',
          'cobalt/testing/browser_tests/run_browser_tests.py'
      ]:
        if os.path.exists(os.path.join(source_dir, f)):
          combined_deps.add(f)

      # Generate run_tests.py
      template_path = os.path.join(
          os.path.dirname(__file__),
          '../testing/browser_tests/tools/run_tests.template.py')
      temp_dir = tempfile.mkdtemp()
      runner_path = os.path.join(temp_dir, 'run_tests.py')
      generate_runner_py(runner_path, target_map, template_path)
      file_lists.append((['run_tests.py'], temp_dir))

    if include_depot_tools:
      vpython_path = shutil.which('vpython3')
      if vpython_path:
        vpython_path = os.path.realpath(vpython_path)
        depot_tools_src = os.path.dirname(vpython_path)
        if os.path.exists(os.path.join(depot_tools_src, 'gclient')):
          logging.info('Bundling depot_tools from %s', depot_tools_src)
          file_lists.append(([os.path.basename(depot_tools_src)],
                             os.path.dirname(depot_tools_src)))
        else:
          logging.warning(
              'vpython3 found at %s but does not appear to be in '
              'depot_tools. Skipping depot_tools bundling.', vpython_path)

    file_lists.append((list(combined_deps), source_dir))

    _make_tar(
        output_path,
        compression,
        compression_level,
        file_lists,
    )


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
      '--compression',
      choices=['xz', 'gz', 'zstd'],
      default='zstd',
      help='The compression algorithm to use.')
  parser.add_argument(
      '--compression-level',
      type=int,
      default=1,
      help='The compression level to use.')
  parser.add_argument(
      '--flatten-deps',
      action='store_true',
      help='Pass this argument to archive files from the source and out '
      'directories both at the root of the deps archive.')
  parser.add_argument(
      '--include-browsertests-runner',
      action='store_true',
      help='Include the browsertest runner script and dependencies.')
  parser.add_argument(
      '--include-depot-tools',
      action='store_true',
      help='Include depot_tools in the archive.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.INFO, format='%(message)s')

  if args.flatten_deps != args.archive_per_target:
    # Relax this check if we are including browsertests runner,
    # as it usually implies a single archive.
    if not args.include_browsertests_runner:
      raise ValueError('Unsupported configuration.')

  create_archive(
      targets=args.targets,
      source_dir=args.source_dir,
      out_dir=args.out_dir,
      destination_dir=args.destination_dir,
      archive_per_target=args.archive_per_target,
      use_android_deps_path=args.use_android_deps_path,
      compression=args.compression,
      compression_level=args.compression_level,
      flatten_deps=args.flatten_deps,
      include_browsertests_runner=args.include_browsertests_runner,
      include_depot_tools=args.include_depot_tools)


if __name__ == '__main__':
  main()

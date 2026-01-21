#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Script to collect test artifacts for cobalt_browsertests into a gzip file.
"""
This script orchestrates the collection of test artifacts for
cobalt_browsertests. It packages necessary binaries, resources, and
dependencies into a tarball.
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] %(message)s')


def find_runtime_deps(build_dir):
  """Finds the runtime_deps file in the build directory."""
  patterns = [
      'cobalt_browsertests__test_runner_script.runtime_deps',
      '*browsertests*.runtime_deps'
  ]
  for pattern in patterns:
    matches = list(Path(build_dir).rglob(pattern))
    if matches:
      return matches[0]
  return None


def get_test_runner(build_dir, is_android):
  """Determines the relative path to the test runner."""
  if is_android:
    return os.path.join('bin', 'run_cobalt_browsertests')

  # For Linux/non-android, we will use the binary directly via
  # run_browser_tests.py. However, some platforms might have a runner script.
  script_rel = os.path.join('bin', 'run_cobalt_browsertests')
  if os.path.isfile(os.path.join(build_dir, script_rel)):
    return script_rel

  return 'cobalt_browsertests'


def copy_fast(src, dst):
  """Fast copy using system cp if possible, falling back to shutil."""
  os.makedirs(os.path.dirname(dst), exist_ok=True)
  try:
    if os.path.isdir(src):
      # Use system cp -a for directories to handle many files efficiently.
      # We copy INTO the parent of dst.
      subprocess.run(['cp', '-a', src, os.path.dirname(dst) + '/'], check=True)
    else:
      # Use cp -a for files too to preserve attributes.
      subprocess.run(['cp', '-a', src, dst], check=True)
  except (subprocess.CalledProcessError, subprocess.SubprocessError, OSError):
    if os.path.isdir(src):
      shutil.copytree(src, dst, symlinks=True, dirs_exist_ok=True)
    else:
      shutil.copy2(src, dst, follow_symlinks=False)


def generate_runner_py(dst_path, target_map):
  """Generates the run_tests.py script inside the archive."""
  template_path = os.path.join(
      os.path.dirname(__file__), 'run_tests.template.py')
  with open(template_path, 'r', encoding='utf-8') as f:
    content = f.read()

    # Inject the target map into the template.

    target_map_repr = repr(target_map)

    content = content.replace('TARGET_MAP = {}',
                              f'TARGET_MAP = {target_map_repr}')

  with open(dst_path, 'w', encoding='utf-8') as f:
    f.write(content)
  os.chmod(dst_path, 0o755)


def main():
  parser = argparse.ArgumentParser(description='Collect test artifacts.')
  parser.add_argument(
      'build_dirs',
      nargs='*',
      default=['out/android-arm_devel'],
      help='Build directories to include (default: out/android-arm_devel)')
  parser.add_argument(
      '-o',
      '--output',
      default='cobalt_browsertests_artifacts.tar.gz',
      help='Output filename')
  args = parser.parse_args()

  target_map = {}

  with tempfile.TemporaryDirectory() as stage_dir:
    src_stage = os.path.join(stage_dir, 'src')
    os.makedirs(src_stage)

    for build_dir in args.build_dirs:
      build_dir = os.path.normpath(build_dir)
      if not os.path.isdir(build_dir):
        logging.warning('Build directory %s not found. Skipping.', build_dir)
        continue

      is_android = 'android' in build_dir.lower()
      runtime_deps_path = find_runtime_deps(build_dir)
      if not runtime_deps_path:
        logging.warning('Could not find runtime_deps in %s. Skipping.',
                        build_dir)
        continue

      test_runner_rel = get_test_runner(build_dir, is_android)
      logging.info('Processing build directory: %s', build_dir)

      # Record target info
      target_name = os.path.basename(build_dir)
      target_map[target_name] = {
          'deps': str(runtime_deps_path),
          'runner': os.path.join(build_dir, test_runner_rel),
          'is_android': is_android
      }

      logging.info('Copying files from %s...', runtime_deps_path)
      with open(runtime_deps_path, 'r', encoding='utf-8') as f:
        for line in f:
          line = line.strip()
          if not line or line.startswith('#'):
            continue
          full_path = os.path.normpath(os.path.join(build_dir, line))
          if os.path.exists(full_path):
            copy_fast(full_path, os.path.join(src_stage, full_path))

      # Ensure the deps file and test runner are included
      copy_fast(
          str(runtime_deps_path), os.path.join(src_stage,
                                               str(runtime_deps_path)))
      test_runner_full = os.path.join(build_dir, test_runner_rel)
      copy_fast(test_runner_full, os.path.join(src_stage, test_runner_full))

    if not target_map:
      logging.error('No valid build directories processed.')
      sys.exit(1)

    # Add run_browser_tests.py specifically
    browser_test_runner = 'cobalt/testing/browser_tests/run_browser_tests.py'
    if os.path.exists(browser_test_runner):
      copy_fast(browser_test_runner, os.path.join(src_stage,
                                                  browser_test_runner))

    # Add essential directories manually (shared across all targets)
    essential_dirs = [
        'build/android', 'build/util', 'build/skia_gold_common', 'testing',
        'third_party/android_build_tools', 'third_party/catapult/common',
        'third_party/catapult/dependency_manager', 'third_party/catapult/devil',
        'third_party/catapult/third_party', 'third_party/logdog',
        'third_party/android_sdk/public/platform-tools'
    ]
    for d in essential_dirs:
      if os.path.isdir(d):
        copy_fast(d, os.path.join(src_stage, d))

    # Add top-level build scripts
    os.makedirs(os.path.join(src_stage, 'build'), exist_ok=True)
    for f in Path('build').glob('*.py'):
      shutil.copy2(f, os.path.join(src_stage, 'build', f.name))

    # Add depot_tools
    vpython_path = shutil.which('vpython3')
    if vpython_path:
      depot_tools_src = os.path.dirname(vpython_path)
      logging.info('Bundling depot_tools from %s', depot_tools_src)
      copy_fast(depot_tools_src, os.path.join(stage_dir, 'depot_tools'))

    # Add .vpython3 files
    if os.path.isfile('.vpython3'):
      shutil.copy2('.vpython3', os.path.join(src_stage, '.vpython3'))
    if os.path.isfile('v8/.vpython3'):
      os.makedirs(os.path.join(src_stage, 'v8'), exist_ok=True)
      shutil.copy2('v8/.vpython3', os.path.join(src_stage, 'v8', '.vpython3'))

    # Generate runner
    generate_runner_py(os.path.join(stage_dir, 'run_tests.py'), target_map)

    logging.info('Creating tarball: %s', args.output)
    subprocess.run(['tar', '-C', stage_dir, '-czf', args.output, '.'],
                   check=True)

  logging.info('Done!')


if __name__ == '__main__':
  main()

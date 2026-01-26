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
  script_rel = os.path.join('bin', 'run_cobalt_browsertests')
  if (is_android or os.path.isfile(os.path.join(build_dir, script_rel))):
    return script_rel

  # For Linux/non-android, we will use the binary directly via
  # run_browser_tests.py. However, some platforms might have a runner script.
  return 'cobalt_browsertests'


def copy_fast(src, dst):
  """Fast copy using system cp if possible, falling back to shutil."""
  dst_parent = os.path.dirname(dst)
  os.makedirs(dst_parent, exist_ok=True)
  try:
    if os.path.isdir(src):
      # Use system cp -af for directories to handle many files efficiently.
      # -a preserves attributes, -f forces overwrite by unlinking if needed.
      subprocess.run(['cp', '-af', src, dst_parent + '/'], check=True)
    else:
      # Use cp -af for files too.
      subprocess.run(['cp', '-af', src, dst], check=True)
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


def copy_if_needed(src, dst, copied_sources):
  """Copies src to dst if src (or its parent) hasn't been copied already."""
  abs_src = os.path.abspath(src)
  if abs_src in copied_sources:
    return
  # Check if any parent directory was already copied.
  parent = os.path.dirname(abs_src)
  while parent and parent != os.path.dirname(parent):
    if parent in copied_sources:
      return
    parent = os.path.dirname(parent)

  if os.path.exists(src):
    copy_fast(src, dst)
    copied_sources.add(abs_src)


def main():
  """Main entrypoint for collecting test artifacts.

  This function:
  1. Parses command-line arguments for build directories and output name.
  2. Identifies runtime dependencies for each target.
  3. Copies binaries, resources, and dependencies to a temporary stage.
  4. Bundles depot_tools and necessary build scripts.
  5. Generates a portable 'run_tests.py' script.
  6. Packages everything into a compressed tarball.
  """
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
  copied_sources = set()

  with tempfile.TemporaryDirectory() as stage_dir:
    src_stage = os.path.join(stage_dir, 'src')
    os.makedirs(src_stage)

    for build_dir in args.build_dirs:
      build_dir = os.path.normpath(build_dir)
      if not os.path.isdir(build_dir):
        logging.warning('Build directory %s not found. Skipping.', build_dir)
        continue

      is_android = 'android' in os.path.basename(build_dir).lower()
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
          'is_android': is_android,
          'build_dir': build_dir
      }

      logging.info('Copying files from %s...', runtime_deps_path)
      with open(runtime_deps_path, 'r', encoding='utf-8') as f:
        for line in f:
          line = line.strip()
          if not line or line.startswith('#'):
            continue
          full_path = os.path.normpath(os.path.join(build_dir, line))
          # Ensure dst is relative to src_stage by stripping leading slash/drive
          rel_path = os.path.relpath(
              full_path, os.sep) if os.path.isabs(full_path) else full_path
          copy_if_needed(full_path, os.path.join(src_stage, rel_path),
                         copied_sources)

      # Ensure the deps file and test runner are included
      rel_deps_path = os.path.relpath(
          str(runtime_deps_path), os.sep) if os.path.isabs(
              str(runtime_deps_path)) else str(runtime_deps_path)
      copy_if_needed(
          str(runtime_deps_path), os.path.join(src_stage, rel_deps_path),
          copied_sources)
      test_runner_full = os.path.join(build_dir, test_runner_rel)
      rel_runner_path = os.path.relpath(
          test_runner_full,
          os.sep) if os.path.isabs(test_runner_full) else test_runner_full
      copy_if_needed(test_runner_full, os.path.join(src_stage, rel_runner_path),
                     copied_sources)

    if not target_map:
      logging.error('No valid build directories processed.')
      sys.exit(1)

    # Add run_browser_tests.py specifically
    browser_test_runner = 'cobalt/testing/browser_tests/run_browser_tests.py'
    rel_browser_runner = os.path.relpath(
        browser_test_runner,
        os.sep) if os.path.isabs(browser_test_runner) else browser_test_runner
    copy_if_needed(browser_test_runner,
                   os.path.join(src_stage, rel_browser_runner), copied_sources)

    # Add essential directories manually (shared across all targets)
    essential_dirs = [
        'build/android', 'build/util', 'build/skia_gold_common', 'testing',
        'third_party/android_build_tools', 'third_party/catapult/common',
        'third_party/catapult/dependency_manager', 'third_party/catapult/devil',
        'third_party/catapult/third_party', 'third_party/logdog',
        'third_party/android_sdk/public/platform-tools'
    ]
    for d in essential_dirs:
      rel_d = os.path.relpath(d, os.sep) if os.path.isabs(d) else d
      copy_if_needed(d, os.path.join(src_stage, rel_d), copied_sources)

    # Add top-level build scripts
    os.makedirs(os.path.join(src_stage, 'build'), exist_ok=True)
    for f in Path('build').glob('*.py'):
      rel_f = os.path.relpath(str(f), os.sep) if os.path.isabs(
          str(f)) else str(f)
      copy_if_needed(str(f), os.path.join(src_stage, rel_f), copied_sources)

    # Add depot_tools
    vpython_path = shutil.which('vpython3')
    if vpython_path:
      depot_tools_src = os.path.dirname(vpython_path)
      logging.info('Bundling depot_tools from %s', depot_tools_src)
      # depot_tools is outside src_stage, so we use copy_fast directly
      # but still track it just in case.
      copy_if_needed(depot_tools_src, os.path.join(stage_dir, 'depot_tools'),
                     copied_sources)

    # Add .vpython3 files
    rel_vpython = os.path.relpath(
        '.vpython3', os.sep) if os.path.isabs('.vpython3') else '.vpython3'
    copy_if_needed('.vpython3', os.path.join(src_stage, rel_vpython),
                   copied_sources)
    rel_v8_vpython = os.path.relpath(
        'v8/.vpython3',
        os.sep) if os.path.isabs('v8/.vpython3') else 'v8/.vpython3'
    copy_if_needed('v8/.vpython3', os.path.join(src_stage, rel_v8_vpython),
                   copied_sources)

    # Generate runner
    generate_runner_py(os.path.join(stage_dir, 'run_tests.py'), target_map)

    # Add Dockerfile to the archive
    dockerfile_src = os.path.join(os.path.dirname(__file__), 'Dockerfile')
    if os.path.exists(dockerfile_src):
      shutil.copy2(dockerfile_src, os.path.join(stage_dir, 'Dockerfile'))

    # Ensure output directory exists
    output_dir = os.path.dirname(os.path.abspath(args.output))
    os.makedirs(output_dir, exist_ok=True)

    logging.info('Creating tarball: %s', args.output)
    subprocess.run(['tar', '-C', stage_dir, '-czf', args.output, '.'],
                   check=True)

  logging.info('Done!')


if __name__ == '__main__':
  main()

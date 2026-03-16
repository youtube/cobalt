#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Script to collect test artifacts for cobalt_browsertests into a gzip file.
"""
This script orchestrates the collection of test artifacts for
cobalt_browsertests. It identifies the runtime dependencies for the
specified build targets, bundles them along with necessary tools
(like depot_tools and Android SDK components), and packages everything
into a portable compressed tarball.
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(levelname)s] %(message)s',
    datefmt='%H:%M:%S')


def find_runtime_deps(build_dir):
  """Finds the runtime_deps file for cobalt_browsertests in the build dir."""
  # Try exact match first
  exact_deps_file = os.path.join(
      build_dir, 'gen.runtime/cobalt/testing/browser_tests/'
      'cobalt_browsertests__test_runner_script.runtime_deps')
  if os.path.isfile(exact_deps_file):
    return Path(exact_deps_file)

  # Fallback to rglob
  results = list(
      Path(build_dir).rglob(
          'cobalt_browsertests__test_runner_script.runtime_deps'))
  if results:
    return results[0]
  return None


def get_test_runner(build_dir, is_android):
  """Returns the relative path to the test runner within the build dir."""
  if is_android:
    return os.path.join('bin', 'run_cobalt_browsertests')

  # For Linux/non-android, we will use the binary directly via
  # run_browser_tests.py. However, some platforms might have a runner script.
  runner_names = [
      os.path.join('bin', 'run_cobalt_browsertests'),
      'cobalt_browsertests_runner',
      'cobalt_browsertests',
  ]
  for name in runner_names:
    if os.path.isfile(os.path.join(build_dir, name)):
      return name

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

  # Optimization: If a parent directory of this file/dir was already copied
  # as a directory, we can skip it.
  parent = os.path.dirname(abs_src)
  while parent and parent != os.path.dirname(parent):
    if parent in copied_sources and os.path.isdir(parent):
      return
    parent = os.path.dirname(parent)

  if os.path.exists(src):
    copy_fast(src, dst)
    copied_sources.add(abs_src)


def main():
  """Main entrypoint for collecting test artifacts."""
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
      help='Output filename (default: cobalt_browsertests_artifacts.tar.gz)')
  parser.add_argument(
      '--output_dir', help='Output directory where the tarball will be placed.')
  parser.add_argument(
      '--compression',
      choices=['xz', 'gz', 'zstd'],
      default='gz',
      help='The compression algorithm to use.')
  parser.add_argument(
      '--adb_platform_tools',
      type=str,
      default=('cobalt/testing/browser_tests/tools/'
               'platform-tools_r33.0.1-linux.zip'),
      help='The zip file to incorporate into the tarball.')
  args = parser.parse_args()

  if args.output_dir:
    if os.path.isabs(args.output):
      parser.error('--output cannot be an absolute path when --output_dir is '
                   'specified.')
    os.makedirs(args.output_dir, exist_ok=True)
    args.output = os.path.join(args.output_dir, args.output)

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

      adb_platform_tools = args.adb_platform_tools
      if is_android and adb_platform_tools:
        copy_if_needed(adb_platform_tools,
                       os.path.join(stage_dir, 'tools/platform-tools.zip'),
                       copied_sources)

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
      modified_deps_lines = []
      with open(runtime_deps_path, 'r', encoding='utf-8') as f:
        for line in f:
          line = line.strip()
          if not line or line.startswith('#'):
            continue

          # Optimization: Prefer stripped libraries for the runner.
          # We copy BOTH unstripped (to tarball) and stripped (to root).
          # We patch the archived runtime_deps to point to stripped root.
          original_rel_path = line
          full_path = os.path.normpath(os.path.join(build_dir, line))

          if is_android and 'lib.unstripped' in line and line.endswith('.so'):
            stripped_name = os.path.basename(line)
            stripped_path = os.path.join(build_dir, stripped_name)
            if os.path.isfile(stripped_path):
              logging.info('Including stripped library: %s', stripped_name)
              # Copy the stripped version to the build root in stage.
              copy_if_needed(stripped_path,
                             os.path.join(src_stage, build_dir, stripped_name),
                             copied_sources)
              # Use the stripped path in our patched deps file.
              original_rel_path = stripped_name

          # Copy the original file (might be unstripped) to its intended path.
          copy_if_needed(full_path, os.path.join(src_stage, full_path),
                         copied_sources)
          modified_deps_lines.append(original_rel_path + '\n')

      # Write the patched runtime_deps to the archive.
      archived_deps_path = os.path.join(src_stage, str(runtime_deps_path))
      os.makedirs(os.path.dirname(archived_deps_path), exist_ok=True)
      with open(archived_deps_path, 'w', encoding='utf-8') as f:
        f.writelines(modified_deps_lines)
      copied_sources.add(os.path.abspath(archived_deps_path))

      # Ensure the test runner is included
      test_runner_full = os.path.join(build_dir, test_runner_rel)
      copy_if_needed(test_runner_full,
                     os.path.join(src_stage, test_runner_full), copied_sources)

    if not target_map:
      logging.error('No valid build directories processed.')
      sys.exit(1)

    # Add run_browser_tests.py specifically
    browser_test_runner = 'cobalt/testing/browser_tests/run_browser_tests.py'
    copy_if_needed(browser_test_runner,
                   os.path.join(src_stage, browser_test_runner), copied_sources)

    # Add essential directories manually (shared across all targets)
    essential_dirs = [
        'build/android', 'build/util', 'build/skia_gold_common', 'testing',
        'third_party/android_build_tools', 'third_party/catapult/common',
        'third_party/catapult/dependency_manager', 'third_party/catapult/devil',
        'third_party/catapult/third_party', 'third_party/logdog',
        'third_party/android_sdk/public/platform-tools',
        'third_party/android_sdk/public/build-tools',
        'third_party/android_platform/development/scripts', 'tools/python',
        'third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer',
        'third_party/llvm-build/Release+Asserts/bin/llvm-objdump'
    ]
    for d in essential_dirs:
      copy_if_needed(d, os.path.join(src_stage, d), copied_sources)

    # Add top-level build scripts
    os.makedirs(os.path.join(src_stage, 'build'), exist_ok=True)
    for f in Path('build').glob('*.py'):
      copy_if_needed(
          str(f), os.path.join(src_stage, 'build', f.name), copied_sources)

    # Add depot_tools
    vpython_path = shutil.which('vpython3')
    if vpython_path:
      depot_tools_src = os.path.dirname(vpython_path)
      logging.info('Bundling depot_tools from %s', depot_tools_src)
      copy_if_needed(depot_tools_src, os.path.join(stage_dir, 'depot_tools'),
                     copied_sources)

    # Add .vpython3 files
    copy_if_needed('.vpython3', os.path.join(src_stage, '.vpython3'),
                   copied_sources)
    copy_if_needed('v8/.vpython3', os.path.join(src_stage, 'v8', '.vpython3'),
                   copied_sources)

    # Generate runner
    generate_runner_py(os.path.join(stage_dir, 'run_tests.py'), target_map)

    logging.info('Creating tarball: %s', args.output)
    if args.compression == 'gz':
      compression_flag = 'gzip -1'
    elif args.compression == 'xz':
      compression_flag = 'xz -T0 -1'
    elif args.compression == 'zstd':
      compression_flag = 'zstd -T0 -1'
    else:
      raise ValueError(f'Unsupported compression: {args.compression}')

    subprocess.run([
        'tar', '-I', compression_flag, '-C', stage_dir, '-cf', args.output, '.'
    ],
                   check=True)

  logging.info('Done!')


if __name__ == '__main__':
  main()

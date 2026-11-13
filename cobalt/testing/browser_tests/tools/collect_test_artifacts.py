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
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(levelname)s] %(message)s',
    datefmt='%H:%M:%S')

_PRUNED_DEPOT_TOOLS_PATHS = (
    '.git', 'external_bin', 'python3', 'python3.bat', 'python3.py',
    'python3_bin_reldir.txt',
    *(f'.cipd_bin/{b}'
      for b in ('.cipd', 'reclient', 'uv', 'dirmd', 'pinpoint', 'crowbar',
                'lucicfg', 'led', 'bb', 'rdb', 'luci', 'luci-auth',
                'luci-auth-ssh-helper', 'luci-auth-ssh-plugin',
                'git-credential-luci', 'prpc')))


def prune_toolchain_stage(src_stage, staged_dt=None):
  """Removes unused catapult and depot_tools paths in --toolchain-only mode."""
  to_remove = [os.path.join(src_stage, 'third_party/catapult/third_party/vinn')]
  if staged_dt and os.path.isdir(staged_dt):
    to_remove.extend(
        os.path.join(staged_dt, p) for p in _PRUNED_DEPOT_TOOLS_PATHS)
    to_remove.extend(Path(staged_dt).glob('bootstrap-*'))
  for p in to_remove:
    if os.path.isdir(p) and not os.path.islink(p):
      shutil.rmtree(p)
    elif os.path.lexists(p):
      os.unlink(p)


def find_runtime_deps(build_dir):
  """Finds the runtime_deps file for cobalt_browsertests in the build dir."""
  deps_by_platform = {
      'android': [
          'gen.runtime/cobalt/testing/browser_tests/',
          'cobalt_browsertests__test_runner_script.runtime_deps'
      ],
      'generic': ['', 'cobalt_browsertests.runtime_deps']
  }
  if 'android' in build_dir:
    platform = 'android'
  else:
    platform = 'generic'

  # Try exact match first
  exact_deps_file = os.path.join(build_dir, *deps_by_platform[platform])
  if os.path.isfile(exact_deps_file):
    return Path(exact_deps_file)

  # Check for _loader variant
  loader_deps_file = os.path.join(build_dir,
                                  'cobalt_browsertests_loader.runtime_deps')
  if os.path.isfile(loader_deps_file):
    return Path(loader_deps_file)

  # Fallback to rglob
  results = list(Path(build_dir).rglob('*browsertests*.runtime_deps'))
  if results:
    return results[0]
  return None


def get_test_runner(build_dir=''):
  """Returns the relative path to the test runner within the build dir."""
  loader_runner = os.path.join('bin', 'run_cobalt_browsertests_loader')
  if build_dir and os.path.isfile(os.path.join(build_dir, loader_runner)):
    return loader_runner
  return os.path.join('bin', 'run_cobalt_browsertests')


def copy_fast(src, dst):
  """Fast copy using system cp if possible, falling back to shutil."""
  dst_parent = os.path.dirname(dst)
  os.makedirs(dst_parent, exist_ok=True)
  try:
    if os.path.isdir(src) and not os.path.islink(src):
      subprocess.run(['cp', '-afT', src, dst], check=True)
    else:
      subprocess.run(['cp', '-af', src, dst], check=True)
  except (subprocess.CalledProcessError, subprocess.SubprocessError, OSError):
    if os.path.isdir(src) and not os.path.islink(src):
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
  parser.add_argument(
      '--toolchain-only',
      action='store_true',
      help='Package only the shared Android host toolchain and build_vars.json.'
  )
  args = parser.parse_args()

  if args.output_dir:
    if os.path.isabs(args.output):
      parser.error('--output cannot be an absolute path when --output_dir is '
                   'specified.')
    os.makedirs(args.output_dir, exist_ok=True)
    args.output = os.path.join(args.output_dir, args.output)

  target_map = {}
  copied_sources = set()
  has_valid_build_dir = False

  with tempfile.TemporaryDirectory() as stage_dir:
    src_stage = os.path.join(stage_dir, 'src')
    os.makedirs(src_stage)

    for build_dir in args.build_dirs:
      build_dir = os.path.normpath(build_dir)
      if not os.path.isdir(build_dir):
        logging.warning('Build directory %s not found. Skipping.', build_dir)
        continue
      rel_build_dir = os.path.relpath(build_dir)
      if not rel_build_dir.startswith('..'):
        build_dir = rel_build_dir

      is_android = any(
          platform_token in os.path.basename(build_dir).lower()
          for platform_token in ('android', 'aosp'))
      runtime_deps_path = (None if args.toolchain_only else
                           find_runtime_deps(build_dir))
      if not args.toolchain_only and not runtime_deps_path:
        logging.warning('Could not find runtime_deps in %s. Skipping.',
                        build_dir)
        continue

      adb_platform_tools = args.adb_platform_tools
      if is_android and adb_platform_tools:
        copy_if_needed(adb_platform_tools,
                       os.path.join(stage_dir, 'tools/platform-tools.zip'),
                       copied_sources)

      if args.toolchain_only:
        staged_build_dir = os.path.join(
            src_stage,
            os.path.join('out', os.path.basename(build_dir))
            if os.path.isabs(build_dir) else build_dir)
        os.makedirs(staged_build_dir, exist_ok=True)
        if is_android:
          copy_if_needed(
              os.path.join(build_dir, 'build_vars.json'),
              os.path.join(staged_build_dir, 'build_vars.json'), copied_sources)
        has_valid_build_dir = True
        continue

      test_runner_rel = get_test_runner(build_dir)
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
            rel_stripped_path = os.path.join(
                os.path.dirname(os.path.dirname(line)), stripped_name)
            stripped_path = os.path.join(build_dir, rel_stripped_path)
            if os.path.isfile(stripped_path):
              logging.info('Including stripped library: %s', rel_stripped_path)
              # Copy the stripped version to its build root dir in stage.
              copy_if_needed(
                  stripped_path,
                  os.path.join(src_stage, build_dir, rel_stripped_path),
                  copied_sources)
              # Use the stripped path in our patched deps file.
              original_rel_path = rel_stripped_path

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

    if not target_map and not has_valid_build_dir:
      logging.error('No valid build directories processed.')
      sys.exit(1)

    # Add run_browser_tests.py specifically
    browser_test_runner = 'cobalt/testing/browser_tests/run_browser_tests.py'
    copy_if_needed(browser_test_runner,
                   os.path.join(src_stage, browser_test_runner), copied_sources)

    # Add essential directories manually (shared across all targets)
    essential_dirs = [
        'build/android', 'build/util', 'build/skia_gold_common', 'testing',
        'third_party/catapult/common',
        'third_party/catapult/dependency_manager', 'third_party/catapult/devil',
        'third_party/catapult/third_party', 'third_party/logdog',
        'third_party/android_sdk/public/platform-tools',
        'third_party/android_sdk/public/build-tools',
        'third_party/android_platform/development/scripts', 'tools/python'
    ]
    if args.toolchain_only:
      essential_dirs.extend([
          'third_party/android_build_tools/bundletool',
          'third_party/android_build_tools/aapt2',
      ])
    else:
      essential_dirs.extend([
          'third_party/android_build_tools',
          'third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer',
          'third_party/llvm-build/Release+Asserts/bin/llvm-objdump',
      ])
    for d in essential_dirs:
      copy_if_needed(d, os.path.join(src_stage, d), copied_sources)

    # Add top-level build scripts
    os.makedirs(os.path.join(src_stage, 'build'), exist_ok=True)
    for f in Path('build').glob('*.py'):
      copy_if_needed(
          str(f), os.path.join(src_stage, 'build', f.name), copied_sources)
    copy_if_needed('build/config/gclient_args.gni',
                   os.path.join(src_stage, 'build/config/gclient_args.gni'),
                   copied_sources)

    # Add depot_tools
    staged_dt = None
    vpython_path = shutil.which('vpython3')
    if vpython_path:
      depot_tools_src = os.path.dirname(vpython_path)
      logging.info('Bundling depot_tools from %s', depot_tools_src)
      staged_dt = os.path.join(stage_dir, 'depot_tools')
      copy_if_needed(depot_tools_src, staged_dt, copied_sources)

    if args.toolchain_only:
      prune_toolchain_stage(src_stage, staged_dt)

    # Add .vpython3 files
    copy_if_needed('.vpython3', os.path.join(src_stage, '.vpython3'),
                   copied_sources)
    copy_if_needed('v8/.vpython3', os.path.join(src_stage, 'v8', '.vpython3'),
                   copied_sources)

    if not args.toolchain_only:
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
        'tar', '--use-compress-program', compression_flag, '-C', stage_dir,
        '-cf', args.output, '.'
    ],
                   check=True)

  logging.info('Done!')


if __name__ == '__main__':
  main()

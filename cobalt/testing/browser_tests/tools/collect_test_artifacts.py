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
import json
import logging
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


def find_runtime_deps(build_dir: Path):
  """Finds the runtime_deps file for cobalt_browsertests in the build dir."""
  # Try exact match first
  exact_deps_file = build_dir / (
      'gen.runtime/cobalt/testing/browser_tests/'
      'cobalt_browsertests__test_runner_script.runtime_deps')
  if exact_deps_file.is_file():
    return exact_deps_file

  # Fallback to rglob
  results = list(
      build_dir.rglob('cobalt_browsertests__test_runner_script.runtime_deps'))
  if results:
    return results[0]
  return None


def get_test_runner(build_dir: Path, is_android: bool):
  """Returns the relative path to the test runner within the build dir."""
  if is_android:
    return Path('bin/run_cobalt_browsertests')

  # For Linux/non-android, we will use the binary directly via
  # run_browser_tests.py. However, some platforms might have a runner script.
  runner_names = [
      Path('bin/run_cobalt_browsertests'),
      Path('cobalt_browsertests_runner'),
      Path('cobalt_browsertests'),
  ]
  for name in runner_names:
    if (build_dir / name).is_file():
      return name

  return Path('cobalt_browsertests')


def copy_fast(src: Path, dst: Path):
  """Fast copy using system cp if possible, falling back to shutil."""
  dst.parent.mkdir(parents=True, exist_ok=True)
  try:
    if src.is_dir():
      # Use system cp -af for directories to handle many files efficiently.
      # -a preserves attributes, -f forces overwrite by unlinking if needed.
      subprocess.run(['cp', '-af', str(src), str(dst.parent) + '/'], check=True)
    else:
      # Use cp -af for files too.
      subprocess.run(['cp', '-af', str(src), str(dst)], check=True)
  except (subprocess.CalledProcessError, subprocess.SubprocessError, OSError):
    if src.is_dir():
      shutil.copytree(src, dst, symlinks=True, dirs_exist_ok=True)
    else:
      shutil.copy2(src, dst, follow_symlinks=False)


def generate_runner_py(dst_path: Path, target_map: dict):
  """Generates the run_tests.py script inside the archive."""
  template_path = Path(__file__).parent / 'run_tests.template.py'
  with template_path.open('r', encoding='utf-8') as f:
    content = f.read()

    # Inject the target map into the template.
    target_map_repr = repr(target_map)

    content = content.replace('TARGET_MAP = {}',
                              f'TARGET_MAP = {target_map_repr}')

  with dst_path.open('w', encoding='utf-8') as f:
    f.write(content)
  dst_path.chmod(0o755)


def copy_if_needed(src: Path, dst: Path, copied_sources: set):
  """Copies src to dst if src (or its parent) hasn't been copied already."""
  abs_src = src.resolve()
  if abs_src in copied_sources:
    return

  # Optimization: If a parent directory of this file/dir was already copied
  # as a directory, we can skip it.
  for parent in abs_src.parents:
    if parent in copied_sources and parent.is_dir():
      return

  if src.exists():
    copy_fast(src, dst)
    copied_sources.add(abs_src)


def create_tarball(output: Path, compression: str, source_dir: Path):
  """Creates a tarball from source_dir."""
  logging.info('Creating tarball: %s', output)
  if compression == 'gz':
    compression_flag = 'gzip -1'
  elif compression == 'xz':
    compression_flag = 'xz -T0 -1'
  elif compression == 'zstd':
    compression_flag = 'zstd -T0 -1'
  else:
    raise ValueError(f'Unsupported compression: {compression}')

  subprocess.run([
      'tar', '-I', compression_flag, '-C',
      str(source_dir), '-cf',
      str(output), '.'
  ],
                 check=True)


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
      '--split',
      action='store_true',
      help='Split artifacts into runner and dynamic build artifacts.')
  parser.add_argument(
      '--runner_output',
      default='cobalt_browsertests_runner.tar.gz',
      help='Output filename for the runner part when --split is used.')

  args = parser.parse_args()

  output_path = Path(args.output)
  runner_output_path = Path(args.runner_output)

  if args.output_dir:
    output_dir = Path(args.output_dir)
    if output_path.is_absolute():
      parser.error('--output cannot be an absolute path when --output_dir is '
                   'specified.')
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / output_path
    if args.runner_output:
      runner_output_path = output_dir / runner_output_path

  target_map = {}
  copied_sources = set()

  with tempfile.TemporaryDirectory() as stage_dir_str:
    stage_dir = Path(stage_dir_str)
    # Use separate staging areas if splitting.
    if args.split:
      runner_stage = stage_dir / 'runner'
      artifacts_stage = stage_dir / 'artifacts'
      runner_stage.mkdir()
      artifacts_stage.mkdir()
      runner_src_stage = runner_stage / 'src'
      artifacts_src_stage = artifacts_stage / 'src'
      runner_src_stage.mkdir()
      artifacts_src_stage.mkdir()
    else:
      # Single stage for everything.
      artifacts_stage = stage_dir
      artifacts_src_stage = stage_dir / 'src'
      artifacts_src_stage.mkdir()
      runner_stage = stage_dir
      runner_src_stage = artifacts_src_stage

    for build_dir_str in args.build_dirs:
      build_dir = Path(build_dir_str)
      if not build_dir.is_dir():
        logging.warning('Build directory %s not found. Skipping.', build_dir)
        continue

      is_android = 'android' in build_dir.name.lower()
      runtime_deps_path = find_runtime_deps(build_dir)
      if not runtime_deps_path:
        logging.warning('Could not find runtime_deps in %s. Skipping.',
                        build_dir)
        continue

      adb_platform_tools = Path(args.adb_platform_tools)
      if is_android and adb_platform_tools:
        copy_if_needed(adb_platform_tools,
                       runner_stage / 'tools/platform-tools.zip',
                       copied_sources)

      test_runner_rel = get_test_runner(build_dir, is_android)
      logging.info('Processing build directory: %s', build_dir)

      # Record target info
      target_name = build_dir.name
      target_map[target_name] = {
          'deps': str(runtime_deps_path.relative_to(build_dir)),
          'runner': str(test_runner_rel),
          'is_android': is_android,
          'build_dir': str(build_dir)
      }

      logging.info('Copying files from %s...', runtime_deps_path)
      modified_deps_lines = []
      with runtime_deps_path.open('r', encoding='utf-8') as f:
        for line in f:
          line = line.strip()
          if not line or line.startswith('#'):
            continue

          # Optimization: Prefer stripped libraries for the runner.
          # We copy BOTH unstripped (to tarball) and stripped (to root).
          # We patch the archived runtime_deps to point to stripped root.
          original_rel_path = line
          full_path = (build_dir / line).resolve()

          if is_android and 'lib.unstripped' in line and line.endswith('.so'):
            stripped_name = Path(line).name
            stripped_path = build_dir / stripped_name
            if stripped_path.is_file():
              logging.info('Including stripped library: %s', stripped_name)
              # Copy the stripped version to the build root in stage.
              copy_if_needed(stripped_path,
                             artifacts_src_stage / build_dir / stripped_name,
                             copied_sources)
              # Use the stripped path in our patched deps file.
              original_rel_path = stripped_name

          # Copy the original file (might be unstripped) to its intended path.
          copy_if_needed(
              full_path,
              artifacts_src_stage / full_path.relative_to(Path.cwd().resolve()),
              copied_sources)
          modified_deps_lines.append(original_rel_path + '\n')

      # Write the patched runtime_deps to the archive.
      archived_deps_path = artifacts_src_stage / runtime_deps_path.resolve(
      ).relative_to(Path.cwd().resolve())
      archived_deps_path.parent.mkdir(parents=True, exist_ok=True)
      with archived_deps_path.open('w', encoding='utf-8') as f:
        f.writelines(modified_deps_lines)
      copied_sources.add(archived_deps_path.resolve())

      # Ensure the test runner is included
      test_runner_full = build_dir / test_runner_rel
      copy_if_needed(test_runner_full, artifacts_src_stage / test_runner_full,
                     copied_sources)

    if not target_map:
      logging.error('No valid build directories processed.')
      sys.exit(1)

    # Add run_browser_tests.py specifically
    browser_test_runner = Path(
        'cobalt/testing/browser_tests/run_browser_tests.py')
    copy_if_needed(browser_test_runner, runner_src_stage / browser_test_runner,
                   copied_sources)

    # Add essential directories manually (shared across all targets)
    essential_dirs = [
        Path('build/android'),
        Path('build/util'),
        Path('build/skia_gold_common'),
        Path('testing'),
        Path('third_party/android_build_tools'),
        Path('third_party/catapult/common'),
        Path('third_party/catapult/dependency_manager'),
        Path('third_party/catapult/devil'),
        Path('third_party/catapult/third_party'),
        Path('third_party/logdog'),
        Path('third_party/android_sdk/public/platform-tools'),
        Path('third_party/android_sdk/public/build-tools'),
        Path('third_party/android_platform/development/scripts'),
        Path('tools/python'),
        Path('third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer'),
        Path('third_party/llvm-build/Release+Asserts/bin/llvm-objdump')
    ]
    for d in essential_dirs:
      copy_if_needed(d, runner_src_stage / d, copied_sources)

    # Add top-level build scripts
    (runner_src_stage / 'build').mkdir(exist_ok=True)
    for f in Path('build').glob('*.py'):
      copy_if_needed(f, runner_src_stage / 'build' / f.name, copied_sources)

    # Add depot_tools
    vpython_path_str = shutil.which('vpython3')
    if vpython_path_str:
      vpython_path = Path(vpython_path_str)
      depot_tools_src = vpython_path.parent
      logging.info('Bundling depot_tools from %s', depot_tools_src)
      copy_if_needed(depot_tools_src, runner_stage / 'depot_tools',
                     copied_sources)

    # Add .vpython3 files
    copy_if_needed(
        Path('.vpython3'), runner_src_stage / '.vpython3', copied_sources)
    copy_if_needed(
        Path('v8/.vpython3'), runner_src_stage / 'v8/.vpython3', copied_sources)

    # Generate runner and target_map.json
    generate_runner_py(runner_stage / 'run_tests.py', target_map)

    if args.split:
      target_map_path = artifacts_stage / 'target_map.json'
      with target_map_path.open('w', encoding='utf-8') as f:
        json.dump(target_map, f, indent=2)

      create_tarball(runner_output_path, args.compression, runner_stage)
      create_tarball(output_path, args.compression, artifacts_stage)
    else:
      create_tarball(output_path, args.compression, stage_dir)

  logging.info('Done!')


if __name__ == '__main__':
  main()

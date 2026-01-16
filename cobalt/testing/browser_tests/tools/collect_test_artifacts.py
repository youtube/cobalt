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

  potential_runners = [
      os.path.join('bin', 'run_cobalt_browsertests'), 'cobalt_browsertests'
  ]
  for runner in potential_runners:
    if os.path.isfile(os.path.join(build_dir, runner)):
      return runner
  return 'cobalt_browsertests'


def copy_fast(src, dst):
  """Fast copy using system cp if possible, falling back to shutil."""
  os.makedirs(os.path.dirname(dst), exist_ok=True)
  if os.path.isdir(src):
    subprocess.call(
        ['cp', '-a', src, os.path.dirname(dst) + '/'],
        stderr=subprocess.DEVNULL)
  else:
    try:
      subprocess.call(['cp', '-a', src, dst], stderr=subprocess.DEVNULL)
    except (subprocess.SubprocessError, OSError):
      shutil.copy2(src, dst, follow_symlinks=False)


def generate_runner_py(dst_path, build_dir, test_runner_rel, runtime_deps_rel):
  """Generates the run_tests.py script inside the archive."""
  content = f"""#!/usr/bin/env python3
import os
import sys
import subprocess
import datetime

def log(msg):
    timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    print(f"[{{timestamp}}] {{msg}}")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # 1. Setup Environment
    log('Configuring environment...')
    depot_tools_path = os.path.join(script_dir, 'depot_tools')
    os.environ['PATH'] = depot_tools_path + os.pathsep + os.environ.get('PATH', '')
    os.environ['DEPOT_TOOLS_UPDATE'] = '0'

    src_dir = os.path.join(script_dir, 'src')
    os.environ['CHROME_SRC'] = src_dir

    # 2. Sanity Checks
    log('Checking for vpython3...')
    vpython_path = shutil_which('vpython3')
    if not vpython_path:
        log('Error: vpython3 not found in bundled depot_tools.')
        sys.exit(1)
    log(f'Using vpython3 at: {{vpython_path}}')

    # 3. Resolve Paths
    deps_path = os.path.join(src_dir, '{runtime_deps_rel}')
    test_runner = os.path.join(src_dir, '{build_dir}', '{test_runner_rel}')

    if not os.path.isfile(deps_path):
        log(f'Error: runtime_deps file not found at {{deps_path}}')
        sys.exit(1)

    if not os.path.isfile(test_runner):
        log(f'Error: test runner not found at {{test_runner}}')
        sys.exit(1)

    # 4. Execute
    log(f'Executing test runner: {{test_runner}}')
    cmd = [vpython_path, test_runner, '--runtime-deps-path', deps_path] + sys.argv[1:]

    try:
        return subprocess.call(cmd)
    except KeyboardInterrupt:
        return 1

def shutil_which(cmd):
    # Simple which implementation
    for path in os.environ['PATH'].split(os.pathsep):
        full = os.path.join(path, cmd)
        if os.path.isfile(full) and os.access(full, os.X_OK):
            return full
    return None

if __name__ == '__main__':
    sys.exit(main())
"""
  with open(dst_path, 'w', encoding='utf-8') as f:
    f.write(content)
  os.chmod(dst_path, 0o755)


def main():
  parser = argparse.ArgumentParser(description='Collect test artifacts.')
  parser.add_argument(
      'build_dir',
      nargs='?',
      default='out/android-arm_devel',
      help='Build directory (default: out/android-arm_devel)')
  parser.add_argument(
      '-o',
      '--output',
      default='cobalt_browsertests_artifacts.tar.gz',
      help='Output filename')
  args = parser.parse_args()

  build_dir = os.path.normpath(args.build_dir)
  if not os.path.isdir(build_dir):
    logging.error('Build directory %s not found.', build_dir)
    sys.exit(1)

  is_android = 'android' in build_dir.lower()
  runtime_deps_path = find_runtime_deps(build_dir)
  if not runtime_deps_path:
    logging.error('Could not find runtime_deps in %s', build_dir)
    sys.exit(1)

  test_runner_rel = get_test_runner(build_dir, is_android)
  logging.info('Using build directory: %s', build_dir)
  logging.info('Found runtime_deps at: %s', runtime_deps_path)

  with tempfile.TemporaryDirectory() as stage_dir:
    src_stage = os.path.join(stage_dir, 'src')
    os.makedirs(src_stage)

    logging.info('Copying files from runtime_deps...')
    with open(runtime_deps_path, 'r', encoding='utf-8') as f:
      for line in f:
        line = line.strip()
        if not line or line.startswith('#'):
          continue
        full_path = os.path.normpath(os.path.join(build_dir, line))
        if os.path.exists(full_path):
          copy_fast(full_path, os.path.join(src_stage, full_path))

    # Add essential directories manually
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

    # Ensure the deps file itself is included
    copy_fast(
        str(runtime_deps_path), os.path.join(src_stage, str(runtime_deps_path)))

    # Add the test runner specifically
    test_runner_full = os.path.join(build_dir, test_runner_rel)
    copy_fast(test_runner_full, os.path.join(src_stage, test_runner_full))

    # Generate runner
    generate_runner_py(
        os.path.join(stage_dir, 'run_tests.py'), build_dir, test_runner_rel,
        str(runtime_deps_path))

    logging.info('Creating tarball: %s', args.output)
    subprocess.call(['tar', '-C', stage_dir, '-czf', args.output, '.'])

  logging.info('Done!')


if __name__ == '__main__':
  main()

#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Generates Cobalt's filtered ICU data bundle (icudtl.dat) at build time."""

import argparse
import glob
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys


def _get_clean_host_env():
  """Returns an environment dictionary stripped of target cross-compile vars."""
  env = os.environ.copy()
  for var in (
      'CC',
      'CXX',
      'CFLAGS',
      'CXXFLAGS',
      'CPPFLAGS',
      'LDFLAGS',
      'AR',
      'RANLIB',
      'NM',
      'LD',
      'STRIP',
      'SDKROOT',
      'IPHONEOS_DEPLOYMENT_TARGET',
      'TVOS_DEPLOYMENT_TARGET',
      'MACOSX_DEPLOYMENT_TARGET',
  ):
    env.pop(var, None)
  return env


def _run_logged(cmd, cwd, env, log_path):
  """Runs a command, logging output to log_path and raising on failure."""
  with open(log_path, 'w', encoding='utf-8') as log_file:
    result = subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        stdout=log_file,
        stderr=subprocess.STDOUT,
        check=False,
    )
  if result.returncode != 0:
    with open(log_path, 'r', encoding='utf-8', errors='replace') as log_file:
      sys.stderr.write(log_file.read())
    raise subprocess.CalledProcessError(result.returncode, cmd)


def main():
  parser = argparse.ArgumentParser(
      description='Generate Cobalt icudtl.dat from filters/cobalt.json.')
  parser.add_argument(
      '--filter-file',
      required=True,
      help='Path to the ICU JSON filter file (e.g. filters/cobalt.json).')
  parser.add_argument(
      '--out-file',
      required=True,
      help='Output path for generated icudtl.dat.')
  parser.add_argument(
      '--build-dir',
      required=True,
      help='Intermediate directory for caching ICU host tools and data build.')
  args = parser.parse_args()

  script_dir = os.path.dirname(os.path.abspath(__file__))
  icu_root = os.path.abspath(os.path.join(script_dir, '..'))
  filter_file = os.path.abspath(args.filter_file)
  out_file = os.path.abspath(args.out_file)
  build_dir = os.path.abspath(args.build_dir)

  os.makedirs(build_dir, exist_ok=True)
  os.makedirs(os.path.dirname(out_file), exist_ok=True)

  host_system = platform.system()
  if host_system == 'Darwin':
    icu_platform = 'MacOSX'
  else:
    icu_platform = 'Linux/gcc'

  num_cores = str(multiprocessing.cpu_count() or 4)
  run_configure_icu = os.path.join(icu_root, 'source', 'runConfigureICU')
  clean_env = _get_clean_host_env()

  # Step 1: Build ICU host tools once per build_dir.
  icupkg_bin = os.path.join(build_dir, 'bin', 'icupkg')
  if not os.path.isfile(icupkg_bin):
    _run_logged(
        [
            run_configure_icu,
            icu_platform,
            '--disable-tests',
            '--disable-samples',
            '--disable-layoutex',
            '--enable-rpath',
            f'--prefix={build_dir}',
        ],
        cwd=build_dir,
        env=clean_env,
        log_path=os.path.join(build_dir, 'configure_tools.log'),
    )
    _run_logged(
        ['make', f'-j{num_cores}'],
        cwd=build_dir,
        env=clean_env,
        log_path=os.path.join(build_dir, 'make_tools.log'),
    )

  # Step 2: Clean data directory, configure with ICU_DATA_FILTER_FILE, and build.
  data_dir = os.path.join(build_dir, 'data')
  if os.path.isdir(data_dir):
    subprocess.run(
        ['make', '-C', 'data', 'clean'],
        cwd=build_dir,
        env=clean_env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )

  data_env = clean_env.copy()
  data_env['ICU_DATA_FILTER_FILE'] = filter_file
  _run_logged(
      [
          run_configure_icu,
          icu_platform,
          '--disable-tests',
          '--disable-samples',
          '--disable-layoutex',
          '--enable-rpath',
          f'--prefix={build_dir}',
      ],
      cwd=build_dir,
      env=data_env,
      log_path=os.path.join(build_dir, 'configure_data.log'),
  )
  _run_logged(
      ['make', f'-j{num_cores}', '-C', 'data'],
      cwd=build_dir,
      env=data_env,
      log_path=os.path.join(build_dir, 'make_data.log'),
  )

  # Step 3: Locate the generated icudt*l.dat file and write atomically.
  candidates = sorted(
      glob.glob(os.path.join(build_dir, 'data', 'out', 'tmp', 'icudt*l.dat')))
  if not candidates:
    sys.stderr.write(
        f'ERROR: No icudt*l.dat found under {build_dir}/data/out/tmp\n')
    return 1

  tmp_out_file = f'{out_file}.tmp.{os.getpid()}'
  shutil.copyfile(candidates[0], tmp_out_file)
  os.replace(tmp_out_file, out_file)
  return 0


if __name__ == '__main__':
  sys.exit(main())

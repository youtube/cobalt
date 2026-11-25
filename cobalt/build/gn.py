#!/usr/bin/env python3
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

# pylint: disable=missing-module-docstring,missing-function-docstring

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple

_BUILDS_DIRECTORY = 'out'

_BUILD_TYPES = {
    'debug': {
        'symbol_level': 2,

        # TODO: b/423038377 - Set is_debug to true and remove
        # cobalt_debugging_enabled once the debug build is fixed.
        'is_debug': 'false',
        'cobalt_debugging_enabled': 'true'
    },
    'devel': {
        'symbol_level': 1,
        'is_debug': 'false'
    },
    'qa': {
        'symbol_level': 1,
        'is_official_build': 'true',
        # Enable stack traces, disabled by is_official_build
        'exclude_unwind_tables': 'false'
    },
    'gold': {
        'symbol_level': 1,
        'is_official_build': 'true',
        'cobalt_is_release_build': 'true'
    }
}

_CHROMIUM_PLATFORMS = [
    'chromium_linux-x64x11',
    'chromium_android-arm',
    'chromium_android-arm64',
    'chromium_android-x86',
    'chromium_ios-arm64',
    'chromium_mac-arm64',
]
_COBALT_STARBOARD_PLATFORMS = [
    'aosp-arm',
    'aosp-arm64',
    'evergreen-arm-softfp',
    'evergreen-arm-softfp-aosp',
    'evergreen-arm64',
    'evergreen-arm64-aosp',
    'evergreen-arm-hardfp-rdk',
    'evergreen-x64',
    'linux-x64x11',
    'linux-x64x11-modular',
    'linux-x64x11-no-starboard',
]
_COBALT_ANDROID_PLATFORMS = [
    'android-arm',
    'android-arm64',
    'android-x86',
]
_COBALT_TVOS_PLATFORMS = [
    'tvos-arm64-device',
    'tvos-arm64-simulator',
]
_RDK_PLATFORMS = frozenset({'evergreen-arm-hardfp-rdk'})
_RDK6_SYSROOT_SUBDIR = 'aarch64-rdk-linux'
_RDK7_SYSROOT_SUBDIR = 'armv8a-rdk-linux'


def _has_rdk_pkgconfig(sysroot_path: str) -> bool:
  return os.path.isdir(os.path.join(sysroot_path, 'usr', 'lib', 'pkgconfig'))


def find_rdk_sysroot(rdk_home: str, rdk_version: Optional[int] = None) -> str:
  """Locates and validates the target sysroot under an RDK SDK root."""
  sysroots_dir = os.path.join(rdk_home, 'sysroots')
  rdk7_path = os.path.join(sysroots_dir, _RDK7_SYSROOT_SUBDIR)
  rdk6_path = os.path.join(sysroots_dir, _RDK6_SYSROOT_SUBDIR)

  if rdk_version == 7:
    if os.path.isdir(rdk7_path) and _has_rdk_pkgconfig(rdk7_path):
      return os.path.abspath(rdk7_path)
    raise ValueError(f'RDK7 target sysroot not found under {sysroots_dir} '
                     f'(expected {_RDK7_SYSROOT_SUBDIR}/usr/lib/pkgconfig).')

  if rdk_version == 6:
    if os.path.isdir(rdk7_path) and not os.path.islink(rdk7_path):
      raise ValueError(
          f'{rdk_home} contains an RDK7 sysroot ({_RDK7_SYSROOT_SUBDIR}), '
          'not an RDK6 toolchain.')
    if os.path.isdir(rdk6_path) and _has_rdk_pkgconfig(rdk6_path):
      return os.path.abspath(rdk6_path)
    raise ValueError(f'RDK6 target sysroot not found under {sysroots_dir} '
                     f'(expected {_RDK6_SYSROOT_SUBDIR}/usr/lib/pkgconfig).')

  # Auto-detect: check non-symlink directories first (RDK7 before RDK6) so a
  # legacy aarch64-rdk-linux -> armv8a-rdk-linux symlink does not mask RDK7.
  candidates = (rdk7_path, rdk6_path)
  for candidate in candidates:
    if (os.path.isdir(candidate) and not os.path.islink(candidate) and
        _has_rdk_pkgconfig(candidate)):
      return os.path.abspath(candidate)
  for candidate in candidates:
    if os.path.islink(candidate) and _has_rdk_pkgconfig(candidate):
      return os.path.abspath(candidate)

  raise ValueError(
      f'Could not locate an RDK target sysroot under {sysroots_dir} '
      f'(expected {_RDK7_SYSROOT_SUBDIR} or {_RDK6_SYSROOT_SUBDIR} with '
      'usr/lib/pkgconfig).')


def _normalize_path(path: str) -> str:
  return os.path.abspath(os.path.expanduser(path))


def _default_rdk_install_dir(rdk_version: int) -> str:
  return _normalize_path(f'~/rdk/toolchain_ah212_{rdk_version}')


def resolve_rdk_toolchain(
    rdk_home_arg: Optional[str],
    rdk_version: Optional[int],
) -> Tuple[str, str]:
  """Resolves (rdk_home_dir, rdk_sysroot) according to CLI/env precedence."""
  if rdk_home_arg:
    rdk_home = _normalize_path(rdk_home_arg)
    return rdk_home, find_rdk_sysroot(rdk_home, rdk_version)

  if rdk_version is not None:
    version_env_var = f'RDK{rdk_version}_HOME'
    env_version_home = os.environ.get(version_env_var)
    if env_version_home:
      rdk_home = _normalize_path(env_version_home)
      return rdk_home, find_rdk_sysroot(rdk_home, rdk_version)

    well_known_dir = _default_rdk_install_dir(rdk_version)
    if os.path.isdir(os.path.join(well_known_dir, 'sysroots')):
      return well_known_dir, find_rdk_sysroot(well_known_dir, rdk_version)

    env_rdk_home = os.environ.get('RDK_HOME')
    if env_rdk_home:
      rdk_home = _normalize_path(env_rdk_home)
      return rdk_home, find_rdk_sysroot(rdk_home, rdk_version)

    if rdk_version == 6 and os.path.isdir('/workspaces/rdk/toolchain/sysroots'):
      rdk_home = '/workspaces/rdk/toolchain'
      return rdk_home, find_rdk_sysroot(rdk_home, rdk_version)

    raise ValueError(
        f'RDK{rdk_version} toolchain not found. Checked {version_env_var}, '
        f'{well_known_dir}, and RDK_HOME. Pass --rdk-home <path> or install '
        f'the RDK{rdk_version} SDK to {well_known_dir}.')

  env_rdk_home = os.environ.get('RDK_HOME')
  if env_rdk_home:
    rdk_home = _normalize_path(env_rdk_home)
    return rdk_home, find_rdk_sysroot(rdk_home, None)

  for candidate in (
      _default_rdk_install_dir(6),
      '/workspaces/rdk/toolchain',
  ):
    if os.path.isdir(os.path.join(candidate, 'sysroots')):
      return candidate, find_rdk_sysroot(candidate, None)

  raise ValueError(
      'RDK toolchain not found. Set RDK_HOME, pass --rdk-home <path>, or '
      f'install the toolchain to {_default_rdk_install_dir(6)}.')


# pylint: disable=too-many-positional-arguments
def write_build_args(build_args_path,
                     platform_args_path,
                     build_type,
                     use_rbe,
                     use_coverage,
                     use_sccache,
                     *,
                     rdk_home_dir: Optional[str] = None,
                     rdk_sysroot: Optional[str] = None):
  """ Write args file, modifying settings for config"""
  gen_comment = '# Set by gn.py'
  with open(build_args_path, 'w', encoding='utf-8') as f:
    if use_rbe:
      f.write(f'use_remoteexec = true {gen_comment}\n')
      f.write(f'use_siso = true {gen_comment}\n')
    if use_sccache:
      f.write(f'cc_wrapper = "sccache" {gen_comment}\n')
    f.write(f'build_type = "{build_type}" {gen_comment}\n')
    for key, value in _BUILD_TYPES[build_type].items():
      f.write(f'{key} = {value} {gen_comment}\n')
    f.write(f'import("//{platform_args_path}")\n')
    if rdk_home_dir:
      f.write(f'rdk_home_dir = {json.dumps(rdk_home_dir, ensure_ascii=False)} '
              f'{gen_comment}\n')
    if rdk_sysroot:
      f.write(f'rdk_sysroot = {json.dumps(rdk_sysroot, ensure_ascii=False)} '
              f'{gen_comment}\n')
    if use_coverage:
      f.write(f'import("//cobalt/build/configs/coverage.gn") {gen_comment}\n')


def configure_out_directory(out_directory: str,
                            platform: str,
                            build_type: str,
                            use_rbe: bool,
                            gn_gen_args: List[str],
                            *,
                            use_coverage: bool,
                            use_sccache: bool,
                            rdk_home_dir: Optional[str] = None,
                            rdk_sysroot: Optional[str] = None):
  Path(out_directory).mkdir(parents=True, exist_ok=True)
  platform_path = f'cobalt/build/configs/{platform}'
  dst_args_gn_file = os.path.join(out_directory, 'args.gn')
  src_args_gn_file = os.path.join(platform_path, 'args.gn')

  if os.path.exists(dst_args_gn_file):
    # Copy the stale args.gn into stale_args.gn
    stale_dst_args_gn_file = dst_args_gn_file.replace('args', 'stale_args')
    os.rename(dst_args_gn_file, stale_dst_args_gn_file)
    print('WARNING: Existing args.gn was overwritten. '
          'Old file was copied to stale_args.gn.')

  write_build_args(
      dst_args_gn_file,
      src_args_gn_file,
      build_type,
      use_rbe,
      use_coverage,
      use_sccache,
      rdk_home_dir=rdk_home_dir,
      rdk_sysroot=rdk_sysroot)

  gn_command = ['gn', 'gen', out_directory] + gn_gen_args
  print('Running', ' '.join(gn_command))
  try:
    subprocess.check_call(gn_command)
  except subprocess.CalledProcessError:
    # A subprocess failed, so don't log the python traceback.
    raise SystemExit(1)  # pylint: disable=raise-missing-from


def parse_args():
  parser = argparse.ArgumentParser()
  builds_directory_group = parser.add_mutually_exclusive_group()
  builds_directory_group.add_argument(
      'out_directory',
      type=str,
      nargs='?',
      help='Path to the directory to build in.')

  parser.add_argument(
      '-p',
      '--platform',
      default='linux-x64x11',
      choices=_CHROMIUM_PLATFORMS + _COBALT_STARBOARD_PLATFORMS +
      _COBALT_ANDROID_PLATFORMS + _COBALT_TVOS_PLATFORMS,
      help='The platform to build.')
  parser.add_argument(
      '-c',
      '-C',
      '--build_type',
      default='devel',
      choices=_BUILD_TYPES.keys(),
      help='The build_type (configuration) to build with.')
  parser.add_argument(
      '--no-check',
      default=False,
      action='store_true',
      help='Pass this flag to disable the header dependency gn check.')
  parser.add_argument(
      '--no-rbe',
      default=False,
      action='store_true',
      help='Pass this flag to disable Remote Build Execution.')
  parser.add_argument(
      '--coverage',
      default=False,
      action='store_true',
      help='Pass this flag to enable code coverage instrumentation.')
  parser.add_argument(
      '--use-sccache',
      default=False,
      action='store_true',
      help='Use sccache for build acceleration.')
  parser.add_argument(
      '--rdk-home',
      type=str,
      default=None,
      help='Path to the RDK SDK toolchain root (RDK platforms only).')
  parser.add_argument(
      '--rdk-version',
      type=int,
      choices=(6, 7),
      default=None,
      help='Target RDK OS major version (6 or 7; RDK platforms only).')

  # Consume --args to avoid passing to gn gen, overriding args.gn file.
  parser.add_argument('--args', help=argparse.SUPPRESS)

  script_args, gen_args = parser.parse_known_args()

  if script_args.platform == 'linux':
    script_args.platform = 'linux-x64x11'

  if script_args.platform not in _RDK_PLATFORMS and (
      script_args.rdk_home is not None or script_args.rdk_version is not None):
    parser.error(
        '--rdk-home and --rdk-version are only supported for RDK platforms.')

  if not script_args.no_check:
    gen_args.append('--check')

  if script_args.args:
    print('WARNING: \'--args\' was ignored to avoid overriding args.gn file.')

  return script_args, gen_args


def main():
  script_args, gen_args = parse_args()
  rdk_home_dir = None
  rdk_sysroot = None
  if script_args.platform in _RDK_PLATFORMS:
    try:
      rdk_home_dir, rdk_sysroot = resolve_rdk_toolchain(script_args.rdk_home,
                                                        script_args.rdk_version)
    except ValueError as err:
      print(f'ERROR: {err}', file=sys.stderr)
      raise SystemExit(1) from err

  if script_args.out_directory:
    builds_out_directory = script_args.out_directory
  elif (script_args.platform == 'evergreen-arm-hardfp-rdk' and
        script_args.rdk_version == 7):
    builds_out_directory = os.path.join(
        _BUILDS_DIRECTORY,
        f'evergreen-arm-hardfp-rdk7_{script_args.build_type}')
  else:
    builds_out_directory = os.path.join(
        _BUILDS_DIRECTORY, f'{script_args.platform}_{script_args.build_type}')
  configure_out_directory(
      builds_out_directory,
      script_args.platform,
      script_args.build_type,
      not script_args.no_rbe,
      gen_args,
      use_coverage=script_args.coverage,
      use_sccache=script_args.use_sccache,
      rdk_home_dir=rdk_home_dir,
      rdk_sysroot=rdk_sysroot)


if __name__ == '__main__':
  main()

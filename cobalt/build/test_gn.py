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
"""
Tests for the gn.py script.
"""

import gn
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

# pylint: disable=inconsistent-quotes


class GnTest(unittest.TestCase):

  def setUp(self):
    self.original_argv = sys.argv
    self.original_cwd = os.getcwd()
    self.test_root_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
    os.chdir(self.test_root_path)

  def tearDown(self):
    sys.argv = self.original_argv
    os.chdir(self.original_cwd)

  @mock.patch('pathlib.Path.mkdir')
  @mock.patch('os.path.exists', return_value=False)
  @mock.patch('os.rename')
  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch('subprocess.check_call')
  def test_coverage_flag(  # pylint: disable=too-many-positional-arguments
      self, mock_check_call, mock_open, mock_rename, mock_exists, mock_mkdir):
    del mock_rename, mock_exists  # unused
    platform = 'android-x86'
    build_type = 'devel'
    expected_out_dir = f'out/{platform}_{build_type}'
    expected_args_content_part = (
        'use_siso = false # Set by gn.py\n'
        'use_remoteexec = true # Set by gn.py\n'
        'rbe_cfg_dir = rebase_path("//cobalt/reclient_cfgs") # Set by gn.py\n'
        'build_type = "devel" # Set by gn.py\n'
        'symbol_level = 1 # Set by gn.py\n'
        'is_debug = false # Set by gn.py\n'
        'import("//cobalt/build/configs/android-x86/args.gn")\n'
        'import("//cobalt/build/configs/coverage.gn") # Set by gn.py\n')

    sys.argv = [
        'gn.py',
        '-p',
        platform,
        '-c',
        build_type,
        '--coverage',
    ]

    gn.main()

    # Verify output directory creation
    mock_mkdir.assert_called_once_with(parents=True, exist_ok=True)

    # Verify args.gn content
    mock_open.assert_called_once()
    handle = mock_open()
    actual_content = "".join(
        [call.args[0] for call in handle.write.call_args_list])
    self.assertIn(expected_args_content_part, actual_content)

    # Verify gn gen command
    mock_check_call.assert_called_once_with([
        'gn',
        'gen',
        expected_out_dir,
        '--check',
    ])

  @mock.patch('pathlib.Path.mkdir')
  @mock.patch('os.path.exists', return_value=False)
  @mock.patch('os.rename')
  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch('subprocess.check_call')
  def test_no_coverage_flag(  # pylint: disable=too-many-positional-arguments
      self, mock_check_call, mock_open, mock_rename, mock_exists, mock_mkdir):
    del mock_rename, mock_exists  # unused
    platform = 'android-x86'
    build_type = 'devel'
    expected_out_dir = f'out/{platform}_{build_type}'
    unexpected_args_content_part = (
        'import("//cobalt/build/configs/coverage.gn") # Set by gn.py\n')

    sys.argv = [
        'gn.py',
        '-p',
        platform,
        '-c',
        build_type,
    ]

    gn.main()

    # Verify output directory creation
    mock_mkdir.assert_called_once_with(parents=True, exist_ok=True)

    # Verify args.gn content
    mock_open.assert_called_once()
    handle = mock_open()
    actual_content = "".join(
        [call.args[0] for call in handle.write.call_args_list])
    self.assertNotIn(unexpected_args_content_part, actual_content)

    # Verify gn gen command
    mock_check_call.assert_called_once_with([
        'gn',
        'gen',
        expected_out_dir,
        '--check',
    ])


class RdkToolchainResolutionTest(unittest.TestCase):
  """Tests for RDK6 and RDK7 side-by-side toolchain resolution in gn.py."""

  def setUp(self):
    self.original_argv = sys.argv
    self.original_cwd = os.getcwd()
    self.test_root_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
    os.chdir(self.test_root_path)
    self.temp_dir = tempfile.mkdtemp()
    self.addCleanup(shutil.rmtree, self.temp_dir)

  def tearDown(self):
    sys.argv = self.original_argv
    os.chdir(self.original_cwd)

  def _create_sdk(self, name, sysroot_subdir, *, with_pkgconfig=True):
    sdk_dir = os.path.join(self.temp_dir, name)
    sysroot = os.path.join(sdk_dir, 'sysroots', sysroot_subdir)
    if with_pkgconfig:
      os.makedirs(os.path.join(sysroot, 'usr', 'lib', 'pkgconfig'))
    else:
      os.makedirs(sysroot)
    return sdk_dir, sysroot

  # User journey: Developer runs gn.py against an RDK7 SDK without creating a
  # manual aarch64-rdk-linux symlink, and gn.py auto-detects armv8a-rdk-linux.
  def test_find_rdk_sysroot_rdk7_only(self):
    sdk7, sysroot7 = self._create_sdk('sdk7', 'armv8a-rdk-linux')
    self.assertEqual(gn.find_rdk_sysroot(sdk7), sysroot7)
    self.assertEqual(gn.find_rdk_sysroot(sdk7, rdk_version=7), sysroot7)
    with self.assertRaises(ValueError):
      gn.find_rdk_sysroot(sdk7, rdk_version=6)

  # User journey: Developer runs gn.py against an RDK6 SDK and expects
  # aarch64-rdk-linux to be selected while --rdk-version 7 fails fast.
  def test_find_rdk_sysroot_rdk6_only(self):
    sdk6, sysroot6 = self._create_sdk('sdk6', 'aarch64-rdk-linux')
    self.assertEqual(gn.find_rdk_sysroot(sdk6), sysroot6)
    self.assertEqual(gn.find_rdk_sysroot(sdk6, rdk_version=6), sysroot6)
    with self.assertRaises(ValueError):
      gn.find_rdk_sysroot(sdk6, rdk_version=7)

  # User journey: Developer already created the RDK7 release-note workaround
  # symlink (aarch64-rdk-linux -> armv8a-rdk-linux); gn.py still resolves the
  # real armv8a-rdk-linux directory and rejects treating it as an RDK6 SDK.
  def test_find_rdk_sysroot_rdk7_with_legacy_symlink(self):
    sdk7, sysroot7 = self._create_sdk('sdk7_sym', 'armv8a-rdk-linux')
    os.symlink('armv8a-rdk-linux',
               os.path.join(sdk7, 'sysroots', 'aarch64-rdk-linux'))
    self.assertEqual(gn.find_rdk_sysroot(sdk7), sysroot7)
    self.assertEqual(gn.find_rdk_sysroot(sdk7, rdk_version=7), sysroot7)
    with self.assertRaises(ValueError):
      gn.find_rdk_sysroot(sdk7, rdk_version=6)

  # User journey: Incomplete or corrupted RDK SDK missing usr/lib/pkgconfig
  # fails fast with an actionable error instead of failing later in pkg-config.
  def test_find_rdk_sysroot_missing_pkgconfig_or_sysroot(self):
    sdk_incomplete, _ = self._create_sdk(
        'sdk_bad', 'armv8a-rdk-linux', with_pkgconfig=False)
    with self.assertRaises(ValueError):
      gn.find_rdk_sysroot(sdk_incomplete)
    empty_sdk = os.path.join(self.temp_dir, 'sdk_empty')
    os.makedirs(empty_sdk)
    with self.assertRaises(ValueError):
      gn.find_rdk_sysroot(empty_sdk)

  # User journey: Developer has RDK6 in RDK_HOME from ~/.bashrc and installs
  # RDK7 at ~/rdk/toolchain_ah212_7; running `gn.py -p evergreen-arm-hardfp-rdk
  # -c qa --rdk-version 7` picks RDK7, pins rdk_home_dir + rdk_sysroot, and
  # generates into out/evergreen-arm-hardfp-rdk7_qa without touching RDK6 out/.
  @mock.patch('pathlib.Path.mkdir')
  @mock.patch('os.path.exists', return_value=False)
  @mock.patch('subprocess.check_call')
  def test_rdk7_version_prefers_well_known_dir_over_rdk6_env(
      self, mock_check_call, mock_exists, mock_mkdir):
    del mock_exists, mock_mkdir
    sdk6, _ = self._create_sdk('rdk6', 'aarch64-rdk-linux')
    fake_home = os.path.join(self.temp_dir, 'home')
    sdk7 = os.path.join(fake_home, 'rdk', 'toolchain_ah212_7')
    sysroot7 = os.path.join(sdk7, 'sysroots', 'armv8a-rdk-linux')
    os.makedirs(os.path.join(sysroot7, 'usr', 'lib', 'pkgconfig'))

    env = {'HOME': fake_home, 'RDK_HOME': sdk6}
    with mock.patch.dict(os.environ, env, clear=True):
      with mock.patch('builtins.open', mock.mock_open()) as m_open:
        sys.argv = [
            'gn.py', '-p', 'evergreen-arm-hardfp-rdk', '-c', 'qa',
            '--rdk-version', '7'
        ]
        gn.main()
        actual_content = ''.join(
            call.args[0] for call in m_open().write.call_args_list)

    self.assertIn(f'rdk_home_dir = "{sdk7}" # Set by gn.py\n', actual_content)
    self.assertIn(f'rdk_sysroot = "{sysroot7}" # Set by gn.py\n',
                  actual_content)
    mock_check_call.assert_called_once_with(
        ['gn', 'gen', 'out/evergreen-arm-hardfp-rdk7_qa', '--check'])

  # User journey: Developer passes --rdk-version 7 when only an RDK6 RDK_HOME
  # is installed; gn.py fails fast instead of silently compiling with RDK6.
  def test_rdk7_version_rejects_rdk6_fallback_env(self):
    sdk6, _ = self._create_sdk('rdk6_only', 'aarch64-rdk-linux')
    fake_home = os.path.join(self.temp_dir, 'empty_home')
    os.makedirs(fake_home)
    with mock.patch.dict(
        os.environ, {
            'HOME': fake_home,
            'RDK_HOME': sdk6
        }, clear=True):
      sys.argv = [
          'gn.py', '-p', 'evergreen-arm-hardfp-rdk', '-c', 'qa',
          '--rdk-version', '7'
      ]
      with self.assertRaises(SystemExit) as ctx:
        gn.main()
      self.assertEqual(ctx.exception.code, 1)

  # User journey: CI or Docker image sets ENV RDK_HOME to an RDK7 toolchain and
  # invokes standard `gn.py -p evergreen-arm-hardfp-rdk -c qa` with no extra
  # flags; gn.py auto-detects armv8a-rdk-linux and keeps the standard out dir.
  @mock.patch('pathlib.Path.mkdir')
  @mock.patch('os.path.exists', return_value=False)
  @mock.patch('subprocess.check_call')
  def test_rdk_home_env_autodetects_rdk7_in_default_out_dir(
      self, mock_check_call, mock_exists, mock_mkdir):
    del mock_exists, mock_mkdir
    sdk7, sysroot7 = self._create_sdk('docker_rdk7', 'armv8a-rdk-linux')
    with mock.patch.dict(os.environ, {'RDK_HOME': sdk7}, clear=True):
      with mock.patch('builtins.open', mock.mock_open()) as m_open:
        sys.argv = ['gn.py', '-p', 'evergreen-arm-hardfp-rdk', '-c', 'qa']
        gn.main()
        actual_content = ''.join(
            call.args[0] for call in m_open().write.call_args_list)

    self.assertIn(f'rdk_home_dir = "{sdk7}" # Set by gn.py\n', actual_content)
    self.assertIn(f'rdk_sysroot = "{sysroot7}" # Set by gn.py\n',
                  actual_content)
    mock_check_call.assert_called_once_with(
        ['gn', 'gen', 'out/evergreen-arm-hardfp-rdk_qa', '--check'])

  # User journey: Explicit --rdk-home overrides RDK7_HOME and RDK_HOME and
  # honors an explicit positional output directory.
  @mock.patch('pathlib.Path.mkdir')
  @mock.patch('os.path.exists', return_value=False)
  @mock.patch('subprocess.check_call')
  def test_explicit_rdk_home_and_out_dir_override_env(self, mock_check_call,
                                                      mock_exists, mock_mkdir):
    del mock_exists, mock_mkdir
    sdk_env, _ = self._create_sdk('env_sdk', 'armv8a-rdk-linux')
    sdk_cli, sysroot_cli = self._create_sdk('cli_sdk', 'armv8a-rdk-linux')
    env = {'RDK_HOME': sdk_env, 'RDK7_HOME': sdk_env}
    with mock.patch.dict(os.environ, env, clear=True):
      with mock.patch('builtins.open', mock.mock_open()) as m_open:
        sys.argv = [
            'gn.py', 'out/custom_rdk7', '-p', 'evergreen-arm-hardfp-rdk', '-c',
            'devel', '--rdk-version', '7', '--rdk-home', sdk_cli
        ]
        gn.main()
        actual_content = ''.join(
            call.args[0] for call in m_open().write.call_args_list)

    self.assertIn(f'rdk_home_dir = "{sdk_cli}" # Set by gn.py\n',
                  actual_content)
    self.assertIn(f'rdk_sysroot = "{sysroot_cli}" # Set by gn.py\n',
                  actual_content)
    mock_check_call.assert_called_once_with(
        ['gn', 'gen', 'out/custom_rdk7', '--check'])

  # User journey: Non-RDK platforms reject --rdk-home and --rdk-version cleanly
  # and never emit undeclared rdk_home_dir or rdk_sysroot GN args.
  def test_non_rdk_platform_rejects_rdk_flags(self):
    for extra in (['--rdk-version', '7'], ['--rdk-home', '/tmp/rdk']):
      sys.argv = ['gn.py', '-p', 'linux-x64x11', '-c', 'devel'] + extra
      with self.assertRaises(SystemExit) as ctx:
        gn.parse_args()
      self.assertEqual(ctx.exception.code, 2)

  # User journey: Developer whose SDK path contains non-ASCII UTF-8 characters
  # gets literal UTF-8 paths in args.gn instead of invalid \uXXXX GN escapes.
  def test_write_build_args_preserves_utf8_rdk_paths(self):
    args_path = os.path.join(self.temp_dir, 'args.gn')
    rdk_home = '/home/josé/rdk/toolchain_ah212_7'
    rdk_sysroot = f'{rdk_home}/sysroots/armv8a-rdk-linux'
    gn.write_build_args(
        args_path,
        'cobalt/build/configs/evergreen-arm-hardfp-rdk/args.gn',
        'qa',
        use_rbe=False,
        use_coverage=False,
        use_sccache=False,
        rdk_home_dir=rdk_home,
        rdk_sysroot=rdk_sysroot)
    with open(args_path, encoding='utf-8') as f:
      content = f.read()
    self.assertIn(f'rdk_home_dir = "{rdk_home}" # Set by gn.py\n', content)
    self.assertIn(f'rdk_sysroot = "{rdk_sysroot}" # Set by gn.py\n', content)
    self.assertNotIn('\\u00e9', content)


if __name__ == '__main__':
  unittest.main()

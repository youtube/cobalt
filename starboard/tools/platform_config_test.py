#!/usr/bin/env python3
#
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
#
"""Tests the command_line module."""

import unittest
import os

from starboard.build.platforms import PLATFORMS
from starboard.tools.build import GetPlatformConfig


class PlatformTest(unittest.TestCase):
  module_dir = os.path.dirname(os.path.realpath(__file__))
  starboard_dir = os.path.abspath(os.path.join(module_dir, '..'))

  def test_loop_all_platforms(self):
    for conf in PLATFORMS:
      config = GetPlatformConfig(conf)
      self.assertEqual(config.GetName(), conf)

  def test_load_get_stub_config(self):
    self.load_get_config('stub', 'stub', has_launcher=False)

  def test_load_get_linux_config(self):
    self.load_get_config(
        'linux-x64x11',
        'linux/x64x11',
        app_env={'ASAN_OPTIONS': 'intercept_tls_get_addr=0'},
        test_filters=[('base_unittests', 'TaskQueueSelectorTest.TestAge')])

  def test_load_get_linux_egl_config(self):
    self.load_get_config(
        'linux-x64x11-egl',
        'linux/x64x11/egl',
        app_env={'ASAN_OPTIONS': 'intercept_tls_get_addr=0'})

  def test_load_get_linux_skia_config(self):
    self.load_get_config(
        'linux-x64x11-skia',
        'linux/x64x11/skia',
        app_env={'ASAN_OPTIONS': 'intercept_tls_get_addr=0'},
        test_filters=[('base_unittests', 'TaskQueueSelectorTest.TestAge')])

  def test_load_get_linux_gcc_config(self):
    self.load_get_config(
        'linux-x64x11-gcc-6-3',
        'linux/x64x11/gcc/6.3',
        app_env={'ASAN_OPTIONS': 'intercept_tls_get_addr=0'})

  def test_load_get_linux_old_clang_config(self):
    self.load_get_config(
        'linux-x64x11-clang-3-9',
        'linux/x64x11/clang/3.9',
        app_env={'ASAN_OPTIONS': 'intercept_tls_get_addr=0'})

  def test_load_android_arm_config(self):
    self.load_get_config(
        'android-arm',
        'android/arm',
        deploy_path=['*.apk'],
        test_filters=[('renderer_test', 'PixelTest.TooManyGlyphs'),
                      ('blackbox', 'deep_links')])

  def test_load_android_arm64_config(self):
    self.load_get_config(
        'android-arm64', 'android/arm64', deploy_path=['*.apk'])

  def test_load_android_arm64_vulkan_config(self):
    self.load_get_config(
        'android-arm64-vulkan', 'android/arm64/vulkan', deploy_path=['*.apk'])

  def test_load_android_x86_config(self):
    self.load_get_config(
        'android-x86',
        'android/x86',
        deploy_path=['*.apk'],
        test_filters=[('renderer_test', 'PixelTest.TooManyGlyphs'),
                      ('net_unittests', 'FILTER_ALL')])

  def test_load_raspi2_config(self):
    self.load_get_config(
        'raspi-2',
        'raspi/2',
        test_filters=[('renderer_test', 'PixelTest.CircularSubPixelBorder')])

  def test_load_raspi2_skia_config(self):
    self.load_get_config(
        'raspi-2-skia',
        'raspi/2/skia',
        test_filters=[('renderer_test', 'PixelTest.CircularSubPixelBorder')])

  def test_load_rdk_config(self):
    self.load_get_config('rdk', 'rdk')

  def test_load_win32_config(self):
    self.load_get_config(
        'win-win32',
        'win/win32',
        test_filters=[('base_unittests', '*TaskScheduler*')])

  def test_load_xb1_config(self):
    self.load_get_config(
        'xb1',
        'xb1',
        deploy_path=['appx/*'],
        test_filters=[('base_unittests', 'PathServiceTest.Get')])

  def test_evergreen_x64_config(self):
    self.load_get_config('evergreen-x64', 'evergreen/x64', test_filters=[])

  def test_evergreen_x86_config(self):
    self.load_get_config('evergreen-x86', 'evergreen/x86')

  def test_evergreen_arm_hardfp_config(self):
    self.load_get_config(
        'evergreen-arm-hardfp',
        'evergreen/arm/hardfp',
        test_filters=[('bindings_test', 'DateBindingsTest.PosixEpoch'),
                      ('renderer_test', 'PixelTest.CircularSubPixelBorder')])

  def test_evergreen_arm_softfp_config(self):
    self.load_get_config('evergreen-arm-softfp', 'evergreen/arm/softfp')

  def test_evergreen_arm64_config(self):
    self.load_get_config('evergreen-arm64', 'evergreen/arm64', test_filters=[])

  def load_get_config(
      self,
      conf: str,
      path: str,
      app_env=None,
      deploy_path=None,
      test_filters=None,
      has_launcher=True,
  ):
    if app_env is None:
      app_env = {}
    if test_filters is None:
      test_filters = []

    conf_dir = os.path.join(self.starboard_dir, path)
    config = GetPlatformConfig(conf)
    self.assertEqual(config.GetName(), conf)
    self.assertEqual(config.GetDirectory(), conf_dir)
    self.assertEqual(config.GetTestEnvVariables(), app_env)
    for test in ['nplb', 'common_test']:
      self.assertIn(test, config.GetTestTargets())
    self.assertEqual(config.GetTestBlackBoxTargets(), [])
    if deploy_path:
      self.assertEqual(config.GetDeployPathPatterns(), deploy_path)
    else:
      with self.assertRaises(NotImplementedError):
        config.GetDeployPathPatterns()
    if has_launcher:
      self.assertIsNotNone(config.GetLauncher())
      self.assertIsNotNone(config.GetLauncherPath())
    else:
      self.assertIsNone(config.GetLauncher())
    app_conf = config.GetApplicationConfiguration('cobalt')
    self.assertEqual(app_conf.GetName(), 'cobalt')
    self.assertEqual(app_conf.GetDirectory(), os.path.join(conf_dir, 'cobalt'))

    filters = app_conf.GetTestFilters()
    self.assertIsInstance(app_conf.GetTestFilters(), list)
    for filt in filters:
      self.assertIsNotNone(filt.test_name)
      self.assertIsNotNone(filt.target_name)
    tuple_filters = [(f.target_name, f.test_name) for f in filters]
    for filt in test_filters:
      self.assertIn(filt, tuple_filters)

    for test in ['base_unittests', 'dom_test']:
      self.assertIn(test, app_conf.GetTestTargets())
    # TODO: This is a bit tricky, but only on Linux
    # self.assertEqual(app_conf.GetTestEnvVariables(),{})
    self.assertIsInstance(app_conf.GetTestEnvVariables(), dict)
    self.assertEqual(app_conf.GetTestBlackBoxTargets(), ['blackbox'])

    wpt = app_conf.GetWebPlatformTestFilters()
    for test in [
        'cors/WebPlatformTest.Run/cors_304_htm',
        'xhr/WebPlatformTest.Run/XMLHttpRequest_status_error_htm'
    ]:
      self.assertIn(test, wpt)
    self.assertIsInstance(wpt, list)

    # Return conf objects for more platform specific testing
    return config, app_conf

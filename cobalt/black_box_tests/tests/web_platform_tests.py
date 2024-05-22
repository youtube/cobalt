# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Tests Cobalt web platforms."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from cobalt.black_box_tests import black_box_tests
from cobalt.black_box_tests.web_platform_test_server import WebPlatformTestServer
from starboard.tools import abstract_launcher
from starboard.tools import build
from starboard.tools.testing import test_filter


class WebPlatformTests(black_box_tests.BlackBoxTestCase):
  """Launch web platform tests."""

  def setUp(self):
    if self.launcher_params.config not in ['debug', 'devel']:
      self.skipTest('Can only run web platform tests on debug or devel config.')

  def test_web_platform(self):
    with WebPlatformTestServer(
        binding_address=self.GetBindingAddress(),
        wpt_http_port=self.GetWptHttpPort()):
      target_params = []

      filters = self.cobalt_config.GetWebPlatformTestFilters()

      # Regardless of our own platform, if we are Evergreen we also need to
      # filter the tests that are filtered by the underlying platform. For
      # example, the 'evergreen-arm-hardfp' needs to filter the 'raspi-2'
      # filtered tests when it is running on a Raspberry Pi 2.
      if self.launcher_params.IsEvergreen():
        loader_platform_config = build.GetPlatformConfig(
            self.launcher_params.loader_platform)
        cobalt_config = loader_platform_config.GetApplicationConfiguration(
            'cobalt')
        for filter_ in cobalt_config.GetWebPlatformTestFilters():
          if filter_ not in filters:
            filters.append(filter_)

      used_filters = []

      for filter_ in filters:
        if filter_ == test_filter.DISABLE_TESTING:
          return
        if filter_ == test_filter.FILTER_ALL:
          return
        if isinstance(filter_, test_filter.TestFilter):
          if filter_.test_name and filter_.test_name == test_filter.FILTER_ALL:
            return
          used_filters.append(filter_.test_name)
        else:
          used_filters.append(filter_)

      if used_filters:
        if 'gtest_filter' not in ' '.join(self.launcher_params.target_params):
          target_params.append(f"--gtest_filter=-{':'.join(used_filters)}")

      if self.launcher_params.target_params:
        target_params += self.launcher_params.target_params

      launcher = abstract_launcher.LauncherFactory(
          self.launcher_params.platform,
          'web_platform_tests',
          self.launcher_params.config,
          device_id=self.launcher_params.device_id,
          target_params=target_params,
          output_file=None,
          out_directory=self.launcher_params.out_directory,
          env_variables={'ASAN_OPTIONS': 'intercept_tls_get_addr=0'},
          loader_platform=self.launcher_params.loader_platform,
          loader_config=self.launcher_params.loader_config,
          loader_out_directory=self.launcher_params.loader_out_directory)
      status = launcher.Run()
      self.assertEqual(status, 0)

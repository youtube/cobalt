# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
"""Starboard tvOS Cobalt configuration."""

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltDarwinTvOSArm64Configuration(
    cobalt_configuration.CobaltConfiguration):
  """Starboard tvOS Cobalt configuration."""

  def WebdriverBenchmarksEnabled(self):
    return False

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetWebPlatformTestFilters(self):
    filters = super().GetWebPlatformTestFilters()
    filters += [
        # Tests do not account for player with url.
        ('csp/WebPlatformTest.Run/'
         'content_security_policy_media_src_media_src_allowed_html'),
        ('csp/WebPlatformTest.Run/'
         'content_security_policy_media_src_media_src_disallowed_html'),
    ]
    return filters

  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'renderer_test': [
          # Flaky test is still being counted as a fail.
          ('RendererPipelineTest.FLAKY_'
           'RasterizerSubmitCalledAtExpectedFrequencyAfterManyPipelineSubmits'),
          ('RendererPipelineTest.FLAKY_'
           'RasterizerSubmitCalledAtExpectedFrequencyAfterSinglePipelineSubmit'
           ),
      ],
  }

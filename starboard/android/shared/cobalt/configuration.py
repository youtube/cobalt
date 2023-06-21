# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Android Cobalt shared configuration."""

from cobalt.build import cobalt_configuration
from starboard.tools.testing import test_filter


class CobaltAndroidConfiguration(cobalt_configuration.CobaltConfiguration):
  """Starboard Android Cobalt shared configuration."""

  def GetTestFilters(self):
    filters = super().GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  def GetTestEnvVariables(self):
    return {
        'base_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'crypto_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        },
        'net_unittests': {
            'ASAN_OPTIONS': 'detect_leaks=0'
        }
    }

  def GetWebPlatformTestFilters(self):
    filters = super().GetWebPlatformTestFilters()
    filters += [
        # Test Name (content-security-policy/media-src/media-src-allowed.html):
        # Allowed media src
        # Disabled because of: Fail (Tests do not account for player with url?).
        ('csp/WebPlatformTest.Run/'
         'content_security_policy_media_src_media_src_allowed_html'),
        ('websockets/WebPlatformTest.Run/websockets_*'),
    ]
    return filters

  # A map of failing or crashing tests per target.
  __FILTERED_TESTS = {  # pylint: disable=invalid-name
      'layout_tests': [
          # Android relies of system fonts and some older Android builds do not
          # have the update (Emoji 11.0) NotoColorEmoji.ttf installed.
          ('CSS3FontsLayoutTests/Layout.Test/'
            'color_emojis_should_render_properly'),

          # Android 12 changed the look of emojis.
          ('CSS3FontsLayoutTests/Layout.Test/'
            '5_2_use_system_fallback_if_no_matching_family_is_found_emoji'),
      ],
      'crypto_unittests': ['P224.*'],
      'renderer_test': [
          # TODO(b/236034292): These tests load the wrong fonts sometimes.
          'PixelTest.SimpleTextInRed40PtChineseFont',
          'PixelTest.SimpleTextInRed40PtThaiFont',

          # The Roboto variable font looks slightly different than the static
          # font version. Android 12 and up use font variations for Roboto.
          'PixelTest.ScalingUpAnOpacityFilterTextDoesNotPixellate',

          # Instead of returning an error when allocating too much texture
          # memory, Android instead just terminates the process.  Since this
          # test explicitly tries to allocate too much texture memory, we cannot
          # run it on Android platforms.
          'StressTest.TooManyTextures',

          # TODO(b/288107692) Failing with NDK 25.2 upgrade, re-enable
          'PixelTest.TooManyGlyphs',
      ],
      'zip_unittests': [
          # These tests, and zipping in general, rely on the ability to iterate
          # recursively to find all of the files that should be zipped. This is
          # explicitly not supported for the asset directory on Android. If this
          # functionality is needed at some point, enabling these tests should
          # be revisited.
          #
          # See: starboard/android/shared/file_internal.cc.
          'ZipTest.Zip*',
          'ZipReaderTest.ExtractToFileAsync_RegularFile',
      ],
  }

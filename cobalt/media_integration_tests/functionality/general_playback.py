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
""" Tests for general playbacks with different formats."""

import logging

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_case import TestCase
from cobalt.media_integration_tests.test_util import MimeStrings, PlaybackUrls


class GeneralPlaybackTest(TestCase):
  """
    Test cases for general playbacks.
  """

  def run_test(self, url, mime=None):
    app = self.CreateCobaltApp(url)
    with app:
      # Skip the test if the mime is not supported.
      if mime and not app.IsMediaTypeSupported(mime):
        logging.info('Mime type (%s) is not supported. Skip the test.', mime)
        return
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for 10 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 10)


TEST_PARAMETERS = [
    ('H264', PlaybackUrls.H264_ONLY, None),
    ('PROGRESSIVE', PlaybackUrls.PROGRESSIVE, None),
    ('ENCRYPTED', PlaybackUrls.ENCRYPTED, None),
    ('VR', PlaybackUrls.VR, None),
    ('VP9', PlaybackUrls.VP9, MimeStrings.VP9),
    ('VP9_HFR', PlaybackUrls.VP9_HFR, MimeStrings.VP9_HFR),
    ('AV1', PlaybackUrls.AV1, MimeStrings.AV1),
    ('AV1_HFR', PlaybackUrls.AV1_HFR, MimeStrings.AV1_HFR),
    ('VERTICAL', PlaybackUrls.VERTICAL, None),
    ('SHORT', PlaybackUrls.SHORT, None),
    ('VP9_HDR_HLG', PlaybackUrls.VP9_HDR_HLG, MimeStrings.VP9_HDR_HLG),
    ('VP9_HDR_PQ', PlaybackUrls.VP9_HDR_PQ, MimeStrings.VP9_HDR_PQ),
    ('HDR_PQ_HFR', PlaybackUrls.HDR_PQ_HFR, MimeStrings.VP9_HDR_PQ_HFR),
]

for name, playback_url, mime_str in TEST_PARAMETERS:
  TestCase.CreateTest(GeneralPlaybackTest, name, GeneralPlaybackTest.run_test,
                      playback_url, mime_str)

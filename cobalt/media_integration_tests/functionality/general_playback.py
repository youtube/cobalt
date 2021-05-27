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

  def run_test_with_url(self, url, mime=None):
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

  def test_h264_playback(self):
    self.run_test_with_url(PlaybackUrls.H264_ONLY)

  def test_encrypted_playback(self):
    self.run_test_with_url(PlaybackUrls.ENCRYPTED)

  def test_vr_playback(self):
    self.run_test_with_url(PlaybackUrls.VR)

  def test_vp9_playback(self):
    self.run_test_with_url(PlaybackUrls.VP9, MimeStrings.VP9)

  def test_vp9_playback_hfr(self):
    self.run_test_with_url(PlaybackUrls.VP9_HFR, MimeStrings.VP9_HFR)

  def test_av1_playback(self):
    self.run_test_with_url(PlaybackUrls.AV1, MimeStrings.AV1)

  def test_av1_playback_hfr(self):
    self.run_test_with_url(PlaybackUrls.AV1_HFR, MimeStrings.AV1_HFR)

  def test_vertical_playback(self):
    self.run_test_with_url(PlaybackUrls.VERTICAL)

  def test_short_playback(self):
    self.run_test_with_url(PlaybackUrls.SHORT)

  def test_hdr_playback_hlg(self):
    self.run_test_with_url(PlaybackUrls.HDR_PQ_HFR, MimeStrings.VP9_HDR_HLG)

  def test_hdr_playback_pq(self):
    self.run_test_with_url(PlaybackUrls.VP9_HDR_PQ, MimeStrings.AV1_HFR)

  def test_hdr_playback_hfr(self):
    self.run_test_with_url(PlaybackUrls.HDR_PQ_HFR, MimeStrings.VP9_HDR_PQ_HFR)

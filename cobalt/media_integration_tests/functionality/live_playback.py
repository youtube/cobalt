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
""" Tests for live playbacks."""

import time

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_case import TestCase
from cobalt.media_integration_tests.test_util import PlaybackUrls


class LivePlaybackTest(TestCase):
  """
    Test cases for live playbacks.
  """

  def run_test_with_url(self, url):
    app = self.CreateCobaltApp(url)
    with app:
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for 5 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 5)
      # Pause the playback and wait for some time.
      app.PlayOrPause()
      app.WaitUntilReachState(
          lambda _app: _app.PlayerState().video_element_state.paused)
      time.sleep(2)
      # Resume the playback.
      app.PlayOrPause()
      app.WaitUntilReachState(
          lambda _app: not _app.PlayerState().video_element_state.paused)
      # Let the playback play for another 5 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 5)

  def test_live_playback(self):
    self.run_test_with_url(PlaybackUrls.LIVE)

  def test_ull_live_playback(self):
    self.run_test_with_url(PlaybackUrls.LIVE_ULL)

  def test_vr_live_playback(self):
    self.run_test_with_url(PlaybackUrls.LIVE_VR)

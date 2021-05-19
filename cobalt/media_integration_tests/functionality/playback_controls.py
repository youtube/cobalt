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
""" Tests for playback control shortcuts."""

import logging
import time

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_app import AdditionalKeys
from cobalt.media_integration_tests.test_case import TestCase


class PlaybackControlsTest(TestCase):
  """
    Test cases for playback control shortcuts.
  """

  def test_play_pause(self):
    app = self.CreateCobaltApp(
        'https://www.youtube.com/tv#/watch?v=RACW52qnJMI')
    with app:
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)
      # Pause the playback.
      logging.info('Pause the playback.')
      app.SendKeys(AdditionalKeys.MEDIA_PLAYPAUSE)
      app.WaitUntilReachState(
          lambda _app: _app.PlayerState().video_element_state.paused)
      # Resume the playback.
      logging.info('Resume the playback.')
      app.SendKeys(AdditionalKeys.MEDIA_PLAYPAUSE)
      app.WaitUntilReachState(
          lambda _app: not _app.PlayerState().video_element_state.paused)
      # Let the playback play for another 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)

  def test_rewind_fastforward(self):
    app = self.CreateCobaltApp(
        'https://www.youtube.com/tv#/watch?v=RACW52qnJMI')
    with app:
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)

      logging.info('Fast forward the playback.')
      old_media_time = app.CurrentMediaTime()
      # The first fastforward will only bring up the progress bar.
      app.SendKeys(AdditionalKeys.MEDIA_FAST_FORWARD)
      # The second fastforward will forward the playback by 10 seconds.
      app.SendKeys(AdditionalKeys.MEDIA_FAST_FORWARD)
      # Press play button to start the playback.
      app.SendKeys(AdditionalKeys.MEDIA_PLAY_PAUSE)
      app.WaitUntilReachState(
          lambda _app: app.CurrentMediaTime() > old_media_time + 10)
      # Let the playback play for another 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)

      logging.info('Rewind the playback.')
      old_media_time = app.CurrentMediaTime()
      # The first rewind will only bring up the progress bar.
      app.SendKeys(AdditionalKeys.MEDIA_REWIND)
      # The second rewind will rewind the playback by 10 seconds.
      app.SendKeys(AdditionalKeys.MEDIA_REWIND)
      # It needs to press play button to start the playback.
      app.SendKeys(AdditionalKeys.MEDIA_PLAY_PAUSE)
      app.WaitUntilReachState(
          lambda _app: app.CurrentMediaTime() < old_media_time)
      # Let the playback play for another 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)

  def test_play_next_and_previous(self):
    app = self.CreateCobaltApp('https://www.youtube.com/tv#/watch?'
        'v=v3dclL2grbs&list=PLEo9nJ_lneYVn57sDqYmx9Bu6K80OauUs')
    with app:
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)
      # Play next track.
      logging.info('Play next playback.')
      app.SendKeys(AdditionalKeys.MEDIA_NEXT_TRACK)
      # Wait until the player is destroyed.
      app.WaitUntilPlayerDestroyed()
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for another 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)
      # Play previous track.
      logging.info('Play previous playback.')
      app.SendKeys(AdditionalKeys.MEDIA_PREV_TRACK)
      # Wait until the player is destroyed.
      app.WaitUntilPlayerDestroyed()
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for another 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)

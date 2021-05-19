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
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=RACW52qnJMI')

  def test_encrypted_playback(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=Vx5lkGS4w30')

  def test_vr_playback(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=Ei0fgLfJ6Tk')

  def test_vp9_playback(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=x7GkebUe6XQ',
                           'video/webm; codecs=vp9')

  def test_vp9_playback_hfr(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=Jsjtt5dWDYU',
                           'video/webm; codecs=vp9; framerate=60')

  def test_av1_playback(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=iXvy8ZeCs5M',
                           'video/mp4; codecs=av01.0.08M.08')

  def test_av1_playback_hfr(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=9jZ01i92JI8',
                           'video/mp4; codecs=av01.0.08M.08; framerate=60')

  def test_vertical_playback(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=jNQXAC9IVRw')

  def test_short_playback(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=NEf8Ug49FEw')

  def test_hdr_playback_hlg(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=ebhEiRWGvZM',
                           'video/webm; codecs=vp09.02.51.10.01.09.18.09.00')

  def test_hdr_playback_pq(self):
    self.run_test_with_url('https://www.youtube.com/tv#/watch?v=Rw-qEKR5uv8',
                           'video/webm; codecs=vp09.02.51.10.01.09.16.09.00')

  def test_hdr_playback_hfr(self):
    self.run_test_with_url(
        'https://www.youtube.com/tv#/watch?v=LXb3EKWsInQ',
        'video/webm; codecs=vp09.02.51.10.01.09.16.09.00; framerate=60')

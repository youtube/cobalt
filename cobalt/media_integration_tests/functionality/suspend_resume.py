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
""" Tests for suspend and resume during playing."""

import logging
import time
import unittest

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_app import Features
from cobalt.media_integration_tests.test_case import TestCase
from cobalt.media_integration_tests.test_util import MimeStrings, PlaybackUrls


class SuspendResumeTest(TestCase):
  """
    Test cases for suspend and resume.
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
      # Let the playback play for 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)
      # Suspend the app and wait the app enters background.
      logging.info('Suspend the application.')
      app.Suspend()
      app.WaitUntilReachState(
          lambda _app: not _app.ApplicationState().is_visible)
      # Wait for 1 second before resume.
      time.sleep(1)
      # Resume the app and let it play for a few time.
      logging.info('Resume the application.')
      app.Resume()
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for 2 seconds.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 2)

  @unittest.skipIf(not TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME),
                   'Suspend and resume is not supported on this platform.')
  def test_h264_playback(self):
    self.run_test_with_url(PlaybackUrls.H264_ONLY)

  @unittest.skipIf(not TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME),
                   'Suspend and resume is not supported on this platform.')
  def test_encrypted_playback(self):
    self.run_test_with_url(PlaybackUrls.ENCRYPTED)

  @unittest.skipIf(not TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME),
                   'Suspend and resume is not supported on this platform.')
  def test_live_stream(self):
    self.run_test_with_url(PlaybackUrls.LIVE)

  @unittest.skipIf(not TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME),
                   'Suspend and resume is not supported on this platform.')
  def test_vp9_playback(self):
    self.run_test_with_url(PlaybackUrls.VP9, MimeStrings.VP9)

  @unittest.skipIf(not TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME),
                   'Suspend and resume is not supported on this platform.')
  def test_av1_playback(self):
    self.run_test_with_url(PlaybackUrls.AV1, MimeStrings.AV1)

  @unittest.skipIf(not TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME),
                   'Suspend and resume is not supported on this platform.')
  def test_vertical_playback(self):
    self.run_test_with_url(PlaybackUrls.VERTICAL)

  @unittest.skipIf(not TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME),
                   'Suspend and resume is not supported on this platform.')
  def test_vr_playback(self):
    self.run_test_with_url(PlaybackUrls.VR)

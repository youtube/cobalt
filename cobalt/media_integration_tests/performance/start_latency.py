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
""" Performance tests of playback operation latency."""

import argparse
import logging
import time

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_app import AdsState
from cobalt.media_integration_tests.test_case import TestCase
from cobalt.media_integration_tests.test_util import MimeStrings, PlaybackUrls


class StartLatencyTest(TestCase):
  """
    Test cases for playback operation latency.
  """

  def __init__(self, *args, **kwargs):
    super(StartLatencyTest, self).__init__(*args, **kwargs)

    parser = argparse.ArgumentParser()
    parser.add_argument('--test_times', default=5, type=int)
    args, _ = parser.parse_known_args()
    self.test_times = args.test_times

  def run_first_start_latency_test(self, test_name, url, mime=None):
    test_results = []
    while len(test_results) < self.test_times:
      app = self.CreateCobaltApp(url)
      with app:
        # Skip the test if the mime is not supported.
        if mime and not app.IsMediaTypeSupported(mime):
          logging.info(' ***** First start latency test result of %s : [n/a].',
                       test_name)
          return
        app.WaitUntilPlayerStart()
        start_time = time.time()
        start_media_time = app.CurrentMediaTime()
        app.WaitUntilReachState(lambda _app: _app.PlayerState(
        ).pipeline_state.first_written_video_timestamp != _app.PlayerState().
                                pipeline_state.last_written_video_timestamp)
        # Record inputs received time to know the preliminary impact of network.
        inputs_received_time = time.time()
        inputs_received_latency = inputs_received_time - start_time
        # Let the playback play for 1 second.
        app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 1)

        # If it's playing ads, wait until ads end and run the test again.
        if app.GetAdsState() != AdsState.NONE:
          app.WaitUntilAdsEnd()
          continue
        media_time = app.CurrentMediaTime() - start_media_time
        end_time = time.time()
        start_latency = end_time - start_time - media_time
        test_results.append([
            start_latency, inputs_received_latency, start_time,
            inputs_received_time, media_time, end_time
        ])
    logging.info(' ***** First start latency test result of %s : %r.',
                 test_name, test_results)

  def run_play_pause_latency_test(self, test_name, url, mime=None):
    app = self.CreateCobaltApp(url)
    with app:
      # Skip the test if the mime is not supported.
      if mime and not app.IsMediaTypeSupported(mime):
        logging.info(' ***** Play/pause latency test result of %s : [n/a].',
                     test_name)
        return
      test_results = []
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      while len(test_results) < self.test_times:
        # Let the playback play for 1 second.
        app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 1)
        start_time = time.time()
        # Pause the playback.
        app.PlayOrPause()
        app.WaitUntilReachState(
            lambda _app: _app.PlayerState().video_element_state.paused)
        pause_latency = time.time() - start_time

        start_time = time.time()
        # Resume the playback.
        app.PlayOrPause()
        app.WaitUntilReachState(
            lambda _app: not _app.PlayerState().video_element_state.paused)

        play_latency = time.time() - start_time
        test_results.append([play_latency, pause_latency])
      logging.info(' ***** Play/pause latency test result of %s : %r.',
                   test_name, test_results)

  def run_fastforward_latency_test(self, test_name, url, mime=None):
    app = self.CreateCobaltApp(url)
    with app:
      # Skip the test if the mime is not supported.
      if mime and not app.IsMediaTypeSupported(mime):
        logging.info(' ***** Fastforward latency test result of %s : [n/a].',
                     test_name)
        return
      test_results = []
      # Wait until the playback starts.
      app.WaitUntilPlayerStart()
      app.WaitUntilAdsEnd()
      # Let the playback play for 1 second.
      app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 1)
      while len(test_results) < self.test_times:
        old_media_time = app.CurrentMediaTime()
        start_time = time.time()
        app.Fastforward()
        app.WaitUntilReachState(lambda _app: _app.PlayerState(
        ).pipeline_state.first_written_video_timestamp != _app.PlayerState().
                                pipeline_state.last_written_video_timestamp)
        # Record inputs received time to know the preliminary impact of network.
        inputs_received_latency = time.time() - start_time
        app.WaitUntilReachState(
            lambda _app: _app.CurrentMediaTime() > old_media_time + 10)
        fastforward_latency = time.time() - start_time
        test_results.append([fastforward_latency, inputs_received_latency])

      logging.info(' ***** Fastforward latency test result of %s : %r.',
                   test_name, test_results)


TEST_PARAMETERS = [
    ('H264', PlaybackUrls.H264_ONLY, MimeStrings.H264),
    ('VP9', PlaybackUrls.VP9, MimeStrings.VP9),
    ('AV1', PlaybackUrls.AV1, MimeStrings.AV1),
    ('VP9_HDR_HLG', PlaybackUrls.VP9_HDR_HLG, MimeStrings.VP9_HDR_HLG),
    ('VP9_HDR_PQ', PlaybackUrls.VP9_HDR_PQ, MimeStrings.VP9_HDR_PQ),
]

for name, playback_url, mime_str in TEST_PARAMETERS:
  TestCase.CreateTest(StartLatencyTest, 'first_start_latency_%s' % (name),
                      StartLatencyTest.run_first_start_latency_test, name,
                      playback_url, mime_str)
  TestCase.CreateTest(StartLatencyTest, 'play_pause_latency_%s' % (name),
                      StartLatencyTest.run_play_pause_latency_test, name,
                      playback_url, mime_str)
  TestCase.CreateTest(StartLatencyTest, 'fastforward_latency_%s' % (name),
                      StartLatencyTest.run_fastforward_latency_test, name,
                      playback_url, mime_str)

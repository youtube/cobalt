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
""" Performance tests of dropped frames."""

import argparse
import logging

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_case import TestCase
from cobalt.media_integration_tests.test_util import MimeStrings, PlaybackUrls


class DroppedFrameTest(TestCase):
  """
    Test cases for playback dropped frames.
  """

  def __init__(self, *args, **kwargs):
    super(DroppedFrameTest, self).__init__(*args, **kwargs)

    parser = argparse.ArgumentParser()
    parser.add_argument('--test_times', default=5, type=int)
    args, _ = parser.parse_known_args()
    self.test_times = args.test_times

  def run_test_with_url(self, test_name, url, mime=None):
    test_results = []
    for _ in range(self.test_times):
      app = self.CreateCobaltApp(url)
      with app:
        # Skip the test if the mime is not supported.
        if mime and not app.IsMediaTypeSupported(mime):
          logging.info(' ***** Dropped frame test result: [%s, n/a]', test_name)
          return
        # Wait until the playback starts.
        app.WaitUntilPlayerStart()
        app.WaitUntilAdsEnd()
        # Let the playback play for 10 seconds.
        app.WaitUntilMediaTimeReached(app.CurrentMediaTime() + 10)

        total_frames = app.PlayerState().video_element_state.total_video_frames
        dropped_frames = app.PlayerState(
        ).video_element_state.dropped_video_frames
        dropped_ratio = dropped_frames * 1.0 / total_frames
        test_results.append([dropped_ratio, dropped_frames, total_frames])
    logging.info(' ***** Dropped frame test result of %s : %r.', test_name,
                 test_results)


TEST_PARAMETERS = [
    ('H264', PlaybackUrls.H264_ONLY, MimeStrings.H264),
    ('VP9', PlaybackUrls.VP9, MimeStrings.VP9),
    ('VP9_HFR', PlaybackUrls.VP9_HFR, MimeStrings.VP9_HFR),
    ('AV1', PlaybackUrls.AV1, MimeStrings.AV1),
    ('AV1_HFR', PlaybackUrls.AV1_HFR, MimeStrings.AV1_HFR),
    ('VP9_HDR_HLG', PlaybackUrls.VP9_HDR_HLG, MimeStrings.VP9_HDR_HLG),
    ('VP9_HDR_PQ', PlaybackUrls.VP9_HDR_PQ, MimeStrings.VP9_HDR_PQ),
    ('VP9_HDR_PQ_HFR', PlaybackUrls.HDR_PQ_HFR, MimeStrings.VP9_HDR_PQ_HFR),
]

for name, playback_url, mime_str in TEST_PARAMETERS:
  TestCase.CreateTest(DroppedFrameTest, name,
                      DroppedFrameTest.run_test_with_url, name, playback_url,
                      mime_str)

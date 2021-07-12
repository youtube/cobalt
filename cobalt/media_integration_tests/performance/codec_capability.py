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
""" Tests of platform codec capabilities."""

import logging

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_case import TestCase
from cobalt.media_integration_tests.test_util import MimeStrings, PlaybackUrls


class CodecCapabilityTest(TestCase):
  """
    Test cases for platform codec capabilities.
  """

  # Returns a string which shows the max supported resolution, or "n/a" if the
  # codec is not supported.
  @staticmethod
  def run_video_codec_test(app, codec_name, codec_mime):
    if app.IsMediaTypeSupported(codec_mime):
      for res_name, _ in reversed(MimeStrings.RESOLUTIONS.items()):
        if app.IsMediaTypeSupported(
            MimeStrings.create_video_mime_string(codec_mime, res_name)):
          return '[%s, %s]' % (codec_name, res_name)
    return '[%s, n/a]' % (codec_name)

  # Returns a string which shows the max supported channels, or "n/a" if the
  # codec is not supported.
  @staticmethod
  def run_audio_codec_test(app, codec_name, codec_mime):
    if app.IsMediaTypeSupported(codec_mime):
      for channels in [6, 4, 2]:
        if app.IsMediaTypeSupported(
            MimeStrings.create_audio_mime_string(codec_mime, channels)):
          return '[%s, %s]' % (codec_name, channels)
    return '[%s, n/a]' % (codec_name)

  def test_video_codec_capability(self):
    app = self.CreateCobaltApp(PlaybackUrls.DEFAULT)
    mimes = [
        ('H264', MimeStrings.H264),
        ('VP9', MimeStrings.VP9),
        ('VP9_HFR', MimeStrings.VP9_HFR),
        ('AV1', MimeStrings.AV1),
        ('AV1_HFR', MimeStrings.AV1_HFR),
        ('VP9_HDR_HLG', MimeStrings.VP9_HDR_HLG),
        ('VP9_HDR_PQ', MimeStrings.VP9_HDR_PQ),
        ('VP9_HDR_PQ_HFR', MimeStrings.VP9_HDR_PQ_HFR),
    ]
    result = [' ***** Video codec capability test results: ']
    with app:
      for mime_name, mime_str in mimes:
        result.append(self.run_video_codec_test(app, mime_name, mime_str))
    logging.info(', '.join(result))

  def test_audio_codec_capability(self):
    app = self.CreateCobaltApp(PlaybackUrls.DEFAULT)
    mimes = [
        ('AAC', MimeStrings.AAC),
        ('OPUS', MimeStrings.OPUS),
    ]
    result = [' ***** Audio codec capability test results: ']
    with app:
      for mime_name, mime_str in mimes:
        result.append(self.run_audio_codec_test(app, mime_name, mime_str))
    logging.info(', '.join(result))

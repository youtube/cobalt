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
"""The module of base media integration test case."""

from collections import OrderedDict

import _env  # pylint: disable=unused-import


class MimeStrings():
  """
    Set of playback mime strings.
  """

  H264 = 'video/mp4; codecs=\\"avc1.4d4015\\"'
  VP9 = 'video/webm; codecs=\\"vp9\\"'
  VP9_HFR = 'video/webm; codecs=\\"vp9\\"; framerate=60'
  AV1 = 'video/mp4; codecs=\\"av01.0.08M.08\\"'
  AV1_HFR = 'video/mp4; codecs=\\"av01.0.08M.08\\"; framerate=60'
  VP9_HDR_HLG = 'video/webm; codecs=\\"vp09.02.51.10.01.09.18.09.00\\"'
  VP9_HDR_PQ = 'video/webm; codecs=\\"vp09.02.51.10.01.09.16.09.00\\"'
  VP9_HDR_PQ_HFR = ('video/webm; codecs=\\"vp09.02.51.10.01.09.16.09.00\\";'
                    'framerate=60')

  AAC = 'audio/mp4; codecs=\\"mp4a.40.2\\"'
  OPUS = 'audio/webm; codecs=\\"opus\\"'

  RESOLUTIONS = OrderedDict([('140P', 'width=256; height=144'),
                             ('240P', 'width=352; height=240'),
                             ('360P', 'width=480; height=360'),
                             ('480P', 'width=640; height=480'),
                             ('720P', 'width=1280; height=720'),
                             ('1080P', 'width=1920; height=1080'),
                             ('2K', 'width=2560; height=1440'),
                             ('4K', 'width=3840; height=2160'),
                             ('8K', 'width=7680; height=4320')])

  @staticmethod
  def create_video_mime_string(codec, resolution):
    return '%s; %s' % (codec, MimeStrings.RESOLUTIONS[resolution])

  @staticmethod
  def create_audio_mime_string(codec, channels):
    return '%s; channels=%d' % (codec, channels)


class PlaybackUrls():
  """
    Set of testing video urls.
  """

  DEFAULT = 'https://www.youtube.com/tv'
  H264_ONLY = 'https://www.youtube.com/tv#/watch?v=RACW52qnJMI'
  PROGRESSIVE = 'https://www.youtube.com/tv#/watch?v=w5tbgPJxyGA'
  ENCRYPTED = 'https://www.youtube.com/tv#/watch?v=iNvUS1dnwfw'

  VP9 = 'https://www.youtube.com/tv#/watch?v=x7GkebUe6XQ'
  VP9_HFR = 'https://www.youtube.com/tv#/watch?v=Jsjtt5dWDYU'
  AV1 = 'https://www.youtube.com/tv#/watch?v=iXvy8ZeCs5M'
  AV1_HFR = 'https://www.youtube.com/tv#/watch?v=9jZ01i92JI8'
  VERTICAL = 'https://www.youtube.com/tv#/watch?v=jNQXAC9IVRw'
  SHORT = 'https://www.youtube.com/tv#/watch?v=NEf8Ug49FEw'
  VP9_HDR_HLG = 'https://www.youtube.com/tv#/watch?v=ebhEiRWGvZM'
  VP9_HDR_PQ = 'https://www.youtube.com/tv#/watch?v=Rw-qEKR5uv8'
  HDR_PQ_HFR = 'https://www.youtube.com/tv#/watch?v=LXb3EKWsInQ'
  VR = 'https://www.youtube.com/tv#/watch?v=Ei0fgLfJ6Tk'

  LIVE = 'https://www.youtube.com/tv#/watch?v=o7UP6i4PAbk'
  LIVE_ULL = 'https://www.youtube.com/tv#/watch?v=KI1XlTQrsa0'
  LIVE_VR = 'https://www.youtube.com/tv#/watch?v=soeo5OPv7CA'

  PLAYLIST = ('https://www.youtube.com/tv#/watch?list='
              'PLynQTqo4blSSCMPUGcH2YnbSrV84AnsRD')
  # The link refers to a video in the middle of the playlist, so that
  # it can be used to test both play previous and next.
  PLAYLIST_MID = ('https://www.youtube.com/tv#/watch?'
                  'v=9WrgRpOJr0I&list=PLynQTqo4blSSCMPUGcH2YnbSrV84AnsRD')

# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Tool to verify video input hashes dump by --dump_video_input_hash."""

import sys


class VideoHashes(object):
  """Manages the timestamp to frame hashes.

     The timestamp to frame hashes are loaded from log file generated using
     --dump_video_input_hash command line parameter.
  """

  def __init__(self, filename):
    """Loads the timestamp to frame hashes dictionary from log file."""
    # A typical log line looks like:
    #   ... filter_based_player_worker_handler.cc(...)] Dump clear audio input \
    #       hash @ 23219: 2952221532
    with open(filename, 'r') as f:
      lines = [
          line for line in f.readlines() if line.find('input hash') != -1 and
          line.find('filter_based_player_worker_handler') != -1
      ]
    is_audio_encrypted = False
    is_video_encrypted = False
    for line in lines:
      if line.find('encrypted audio') != -1:
        is_audio_encrypted = True
        continue
      if line.find('encrypted video') != -1:
        is_video_encrypted = True
        continue

      tokens = line.split('Dump clear')
      assert len(tokens) > 1

      tokens = tokens[-1].split()
      assert len(tokens) == 6

      if tokens[0] == 'audio':
        self._audio_timestamp_to_hashes[tokens[4]] = [tokens[5]]
      else:
        assert tokens[0] == 'video'
        self._video_timestamp_to_hashes[tokens[4]] = [tokens[5]]

    print '{} contains {} audio samples and {} video samples,'.format(
        filename, len(self._audio_timestamp_to_hashes),
        len(self._video_timestamp_to_hashes))
    print '  its audio stream is {}, its video stream is {}.'.format(
        'encrypted' if is_audio_encrypted else 'clear',
        'encrypted' if is_video_encrypted else 'clear')

  def AreHashesMatching(self, other):
    """Compare if two objects match each other."""
    self._AreHashesMatching('audio', self._audio_timestamp_to_hashes,
                            other.GetAudioTimestampToHashes())
    self._AreHashesMatching('video', self._video_timestamp_to_hashes,
                            other.GetVideoTimestampToHashes())

  def _AreHashesMatching(self, media_type, timestamp_to_hashes1,
                         timestamp_to_hashes2):
    """Compare if two timestamp to frame hashes dictionary match each other."""
    hashes_to_check = min(len(timestamp_to_hashes1), len(timestamp_to_hashes2))

    all_matches = True

    for timestamp in timestamp_to_hashes1:
      hashes_to_check = hashes_to_check - 1
      if hashes_to_check < 0:
        break

      if timestamp not in timestamp_to_hashes2:
        print media_type, 'frame @ timestamp', timestamp, ('in file 1 isn\'t in'
                                                           ' file 2')
        all_matches = False
        continue
      if timestamp_to_hashes1[timestamp] != timestamp_to_hashes2[timestamp]:
        print media_type, 'frame @ timestamp', timestamp, 'hash mismatches'
        all_matches = False
        continue

    if all_matches:
      print 'All', media_type, 'inputs match'

  def GetAudioTimestampToHashes(self):
    return self._audio_timestamp_to_hashes

  def GetVideoTimestampToHashes(self):
    return self._video_timestamp_to_hashes

  _audio_timestamp_to_hashes = {}
  _video_timestamp_to_hashes = {}


def CompareVideoHashes(filename1, filename2):
  hashes1 = VideoHashes(filename1)
  hashes2 = VideoHashes(filename2)

  hashes1.AreHashesMatching(hashes2)


if len(sys.argv) != 3:
  print('USAGE: compare_video_timestamp_to_hashes.py <input_file_1>',
        '<input_file_2>')
else:
  CompareVideoHashes(sys.argv[1], sys.argv[2])

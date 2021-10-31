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
""" Endurance test of playbacks."""

import argparse
import logging
import random
import time

import _env  # pylint: disable=unused-import
from cobalt.media_integration_tests.test_app import Features, MediaSessionPlaybackState
from cobalt.media_integration_tests.test_case import TestCase
from cobalt.media_integration_tests.test_util import PlaybackUrls

# The variables below are all in seconds.
STATUS_CHECK_INTERVAL = 0.5
SINGLE_PLAYBACK_WATCH_TIME_MAXIMUM = 2.0 * 60 * 60
PLAYER_INITIALIZATION_WAITING_TIMEOUT = 5.0 * 60
MEDIA_TIME_UPDATE_WAITING_TIMEOUT = 10.0
WRITTEN_INPUT_WAITING_TIMEOUT = 30.0
PLAYBACK_END_WAITING_TIMEOUT = 30.0
# Needs to interact with the app regularly to keep it active. Otherwise, the
# playback may be paused.
IDLE_TIME_MAXIMUM = 60 * 60


class EnduranceTest(TestCase):
  """
    Test case for playback endurance test.
  """

  def __init__(self, *args, **kwargs):
    super(EnduranceTest, self).__init__(*args, **kwargs)

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--startup_url', default=PlaybackUrls.PLAYLIST, type=str)
    # Stop the test after |max_running_time| (in hours). If |max_running_time|
    # is 0 or negative, the test will not stop until it gets an error.
    parser.add_argument('--max_running_time', default=20, type=float)
    # Insert random actions if |random_action_interval| is greater than 0.
    parser.add_argument('--random_action_interval', default=0.0, type=float)
    args, _ = parser.parse_known_args()

    self.startup_url = args.startup_url
    # Convert |max_running_time| to seconds.
    self.max_running_time = args.max_running_time * 60 * 60
    self.needs_random_action = args.random_action_interval > 0
    self.random_action_interval = args.random_action_interval

  def ResetTimestamps(self):
    self.last_media_time = -1
    self.last_media_time_update_time = -1
    self.last_written_audio_timestamp = -1
    self.last_written_audio_update_time = -1
    self.last_written_video_timestamp = -1
    self.last_written_video_update_time = -1
    self.audio_eos_written_time = -1
    self.video_eos_written_time = -1
    self.playback_end_time = -1

  def OnPlayerStateChanged(self, player_state_handler, player_state):
    current_running_time = time.time() - self.start_time
    element_state = player_state.video_element_state
    pipeline_state = player_state.pipeline_state
    media_session_state = player_state.media_session_state

    if not player_state_handler.IsPlayerPlaying():
      return

    # Reset timestamps after player identifier is changed.
    if self.player_identifier != pipeline_state.identifier:
      self.player_identifier = pipeline_state.identifier
      self.playback_start_time = current_running_time
      self.ResetTimestamps()

    # Reset timestamps after resume. It could happen after pause, fastforward,
    # rewind and playback switch.
    if media_session_state.playback_state != MediaSessionPlaybackState.PLAYING:
      self.playback_is_playing = False
    elif not self.playback_is_playing:
      self.playback_is_playing = True
      self.ResetTimestamps()

    # Update media time.
    if self.last_media_time != element_state.current_time:
      self.last_media_time = element_state.current_time
      self.last_media_time_update_time = current_running_time

    # Update written audio timestamp.
    if (self.last_written_audio_timestamp !=
        pipeline_state.last_written_audio_timestamp):
      self.last_written_audio_timestamp = (
          pipeline_state.last_written_audio_timestamp)
      self.last_written_audio_update_time = current_running_time

    # Update written video timestamp.
    if (self.last_written_video_timestamp !=
        pipeline_state.last_written_video_timestamp):
      self.last_written_video_timestamp = (
          pipeline_state.last_written_video_timestamp)
      self.last_written_video_update_time = current_running_time

    # Update audio eos timestamp.
    if (self.audio_eos_written_time == -1 and
        pipeline_state.is_audio_eos_written):
      self.audio_eos_written_time = current_running_time

    # Update video eos timestamp.
    if (self.video_eos_written_time == -1 and
        pipeline_state.is_video_eos_written):
      self.video_eos_written_time = current_running_time

    # Update playback end time.
    if (self.audio_eos_written_time != -1 and
        self.video_eos_written_time != -1 and self.playback_end_time == -1 and
        element_state.current_time >= element_state.duration):
      self.playback_end_time = current_running_time

  def GenerateErrorString(self, err_msg):
    return (
        '%s (running time: %f, player identifier: "%s", is playing: %r, '
        'playback start time: %f, last media time: %f (updated at %f), '
        'last written audio timestamp: %d (updated at %f), '
        'last written video timestamp: %d (updated at %f), '
        'audio eos written at %f, video eos written at %f, '
        'playback ended at %f).' %
        (err_msg, time.time() - self.start_time, self.player_identifier,
         self.playback_is_playing, self.playback_start_time,
         self.last_media_time, self.last_media_time_update_time,
         self.last_written_audio_timestamp, self.last_written_audio_update_time,
         self.last_written_video_timestamp, self.last_written_video_update_time,
         self.audio_eos_written_time, self.video_eos_written_time,
         self.playback_end_time))

  def SendRandomAction(self, app):

    def SuspendAndResume(app):
      app.Suspend()
      app.WaitUntilReachState(
          lambda _app: not _app.ApplicationState().is_visible)
      # Wait for 1 second before resume.
      time.sleep(1)
      app.Resume()

    actions = {
        'PlayPause': lambda _app: _app.PlayOrPause(),
        'Fastforward': lambda _app: _app.Fastforward(),
        'Rewind': lambda _app: _app.Rewind(),
        'PlayPrevious': lambda _app: _app.PlayPrevious(),
        'PlayNext': lambda _app: _app.PlayNext(),
    }
    if TestCase.IsFeatureSupported(Features.SUSPEND_AND_RESUME):
      actions['SuspendAndResume'] = SuspendAndResume

    action_name = random.choice(list(actions.keys()))
    logging.info('Send random action (%s).', action_name)
    actions[action_name](app)

  def test_playback_endurance(self):
    self.start_time = time.time()
    self.player_identifier = ''
    self.playback_is_playing = False
    self.playback_start_time = -1
    self.last_state_check_time = 0
    self.last_action_time = 0
    self.ResetTimestamps()

    app = self.CreateCobaltApp(self.startup_url)
    app.AddPlayerStateChangeHandler(self.OnPlayerStateChanged)
    with app:
      while True:
        # Put the sleep at the top of the loop.
        time.sleep(STATUS_CHECK_INTERVAL)

        current_running_time = time.time() - self.start_time
        # Stop the test after reaching max running time.
        if (self.max_running_time > 0 and
            current_running_time > self.max_running_time):
          break
        # Skip if there's no running player.
        if not app.player_state_handler.IsPlayerPlaying():
          # TODO: identify network problem
          self.assertTrue(
              current_running_time - self.last_state_check_time <
              PLAYER_INITIALIZATION_WAITING_TIMEOUT,
              self.GenerateErrorString(
                  'Timed out waiting for player initialization (waited: %f).' %
                  (current_running_time - self.last_state_check_time)))
          continue
        self.last_state_check_time = current_running_time
        # Skip to next playback if it has been played for long time.
        if (self.playback_start_time != -1 and
            current_running_time - self.playback_start_time >
            SINGLE_PLAYBACK_WATCH_TIME_MAXIMUM):
          app.PlayNext()
          # Set start time to -1 here to avoid send same command in next loop.
          self.playback_start_time = -1
          continue
        if self.playback_is_playing:
          # Check media time.
          if self.last_media_time_update_time > 0:
            # TODO: identify network problem
            self.assertTrue(
                current_running_time - self.last_media_time_update_time <
                MEDIA_TIME_UPDATE_WAITING_TIMEOUT,
                self.GenerateErrorString(
                    'Timed out waiting for media time update (waited: %f).' %
                    (current_running_time - self.last_media_time_update_time)))
          # Check written audio timestamp.
          if (self.last_written_audio_update_time > 0 and
              self.audio_eos_written_time == -1):
            self.assertTrue(
                current_running_time - self.last_written_audio_update_time <
                WRITTEN_INPUT_WAITING_TIMEOUT,
                self.GenerateErrorString(
                    'Timed out waiting for new audio input (waited: %f).' %
                    (current_running_time -
                     self.last_written_audio_update_time)))
          # Check written video timestamp.
          if (self.last_written_video_update_time > 0 and
              self.video_eos_written_time == -1):
            self.assertTrue(
                current_running_time - self.last_written_video_update_time <
                WRITTEN_INPUT_WAITING_TIMEOUT,
                self.GenerateErrorString(
                    'Timed out waiting for new video input (waited: %f).' %
                    (current_running_time -
                     self.last_written_video_update_time)))
          # Check if the playback ends properly.
          if (self.audio_eos_written_time > 0 and
              self.video_eos_written_time > 0 and self.playback_end_time > 0):
            self.assertTrue(
                current_running_time - self.playback_end_time <
                PLAYBACK_END_WAITING_TIMEOUT,
                self.GenerateErrorString(
                    'Timed out waiting for playback to end (waited: %f).' %
                    (current_running_time - self.playback_end_time,)))

        # Send random actions.
        if (self.needs_random_action and
            current_running_time - self.last_action_time >
            self.random_action_interval):
          self.SendRandomAction(app)
          self.last_action_time = current_running_time

        # Interact with the app to keep it active.
        if current_running_time - self.last_action_time > IDLE_TIME_MAXIMUM:
          # Play the previous playback again.
          app.PlayPrevious()
          self.last_action_time = current_running_time

      app.RemovePlayerStateChangeHandler(self.OnPlayerStateChanged)

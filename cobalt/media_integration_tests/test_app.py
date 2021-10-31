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
"""A module to support fundamental communications with Cobalt application."""

import json
import logging
import threading
import time

import _env  # pylint: disable=unused-import
from cobalt.tools.automated_testing import cobalt_runner
from cobalt.tools.automated_testing import webdriver_utils

webdriver_keys = webdriver_utils.import_selenium_module('webdriver.common.keys')

PERIODIC_QUERIES_INTERVAL_SECONDS = 0.1
THREAD_EXIT_TIMEOUT_SECONDS = 10
WAIT_INTERVAL_SECONDS = 0.5
WAIT_UNTIL_REACH_STATE_DEFAULT_TIMEOUT_SECONDS = 30
WAIT_UNTIL_ADS_END_DEFAULT_TIMEOUT_SECONDS = 120
WAIT_UNTIL_MEDIA_TIME_REACHED_DEFAULT_TIMEOUT_SECONDS = 30

ACCOUNT_SELECTOR_ADD_ACCOUNT_TEXT = u'Add account'


def GetValueFromQueryResult(query_result, key, default):
  if query_result is None:
    return default

  if not isinstance(query_result, dict):
    return default

  if not query_result.has_key(key):
    return default

  value = query_result[key]
  default_value_type = type(default)
  value_type = type(value)

  if value is None:
    return default

  if value_type == default_value_type:
    return value

  if value_type in (int, float) and default_value_type in (int, float):
    return value

  # CVal returns unicode, so we need to decode them before using.
  if default_value_type in (bool, int, float):
    if value_type in (str, unicode):
      try:
        # Try to parse numbers and booleans.
        parsed_value = json.loads(value)
      except ValueError:
        raise RuntimeError('Failed to parse query result.')

      return parsed_value

  elif default_value_type is str:
    if value_type == unicode:
      return value.encode('utf-8')

  raise NotImplementedError('Convertion from (%s) to (%s) is not suppoted.' %
                            (value_type, default_value_type))


class AdditionalKeys():
  """
    Set of special keys codes for media control, corresponding to cobalt
    webdriver AdditionalSpecialKey.
  """
  MEDIA_NEXT_TRACK = u'\uf000'
  MEDIA_PREV_TRACK = u'\uf001'
  MEDIA_STOP = u'\uf002'
  MEDIA_PLAY_PAUSE = u'\uf003'
  MEDIA_REWIND = u'\uf004'
  MEDIA_FAST_FORWARD = u'\uf005'


class Features():
  """Set of platform features."""
  SUSPEND_AND_RESUME = 1
  SEND_KEYS = 2


class ApplicationState():
  """Set of platform features."""

  def __init__(self, query_result=None):
    if not query_result is None and not isinstance(query_result, dict):
      raise NotImplementedError

    self.is_visible = GetValueFromQueryResult(query_result, 'is_visible', False)

  def __eq__(self, other):
    if not isinstance(other, self.__class__):
      raise NotImplementedError
    return self.is_visible == other.is_visible

  def __ne__(self, other):
    return not self.__eq__(other)

  @staticmethod
  def QueryJsCode():
    return """
      return {
        is_visible: document.visibilityState === "visible",
      }
    """


class ApplicationStateHandler():
  """The handler of applicatoin state."""

  def __init__(self, app):
    self.app = app
    self.state_change_handlers = []
    self.app_state = ApplicationState()

    self.app.RegisterPeriodicQuery('Application',
                                   ApplicationState.QueryJsCode(),
                                   self._ApplicatinStateQueryHandler)

  def AddAppStateChangeHandler(self, handler):
    self.state_change_handlers.append(handler)

  def RemoveAppStateChangeHandler(self, handler):
    self.state_change_handlers.remove(handler)

  def _NotifyHandlersOnStateChanged(self):
    for handler in self.state_change_handlers:
      handler(self, self.app, self.app_state)

  def _ApplicatinStateQueryHandler(self, app, query_name, query_result):
    del app, query_name  # Unused here.
    # Note that the callback is invoked on a different thread.
    new_state = ApplicationState(query_result)
    if self.app_state != new_state:
      self.app_state = new_state
      self._NotifyHandlersOnStateChanged()


class PipelinePlayerState(int):
  """Set of pipeline states, equals to SbPlayerState."""
  INITIALIZED = 0  # kSbPlayerStateInitialized
  PREROLLING = 1  # kSbPlayerStatePrerolling
  PRESENTING = 2  # kSbPlayerStatePresenting
  ENDOFSTREAM = 3  # kSbPlayerStateEndOfStream
  DESTROYED = 4  # kSbPlayerStateDestroyed


class PipelineState():
  """
    The states of SbPlayerPipeline. We update the states periodically via
    webdriver and logs. Note that the duration and timestamps are in
    milliseconds.
  """

  def __init__(self, query_result=None):
    # If there's no player existing, the query return unicode code "null".
    if (not query_result is None and not query_result == u'null' and
        not isinstance(query_result, dict)):
      raise NotImplementedError

    self.identifier = GetValueFromQueryResult(query_result, 'identifier', '')
    self.is_started = GetValueFromQueryResult(query_result, 'is_started', False)
    self.is_suspended = GetValueFromQueryResult(query_result, 'is_suspended',
                                                False)
    self.is_stopped = GetValueFromQueryResult(query_result, 'is_stopped', False)
    self.is_ended = GetValueFromQueryResult(query_result, 'is_ended', False)
    self.player_state = PipelinePlayerState(
        GetValueFromQueryResult(query_result, 'player_state', 0))
    self.volume = GetValueFromQueryResult(query_result, 'volume', 1.0)
    self.playback_rate = GetValueFromQueryResult(query_result, 'playback_rate',
                                                 1.0)
    self.duration = GetValueFromQueryResult(query_result, 'duration', 0)
    self.last_media_time = GetValueFromQueryResult(query_result,
                                                   'last_media_time', 0)
    self.max_video_capabilities = GetValueFromQueryResult(
        query_result, 'max_video_capabilities', '')
    self.seek_time = GetValueFromQueryResult(query_result, 'seek_time', 0)
    self.first_written_audio_timestamp = GetValueFromQueryResult(
        query_result, 'first_written_audio_timestamp', 0)
    self.first_written_video_timestamp = GetValueFromQueryResult(
        query_result, 'first_written_video_timestamp', 0)
    self.last_written_audio_timestamp = GetValueFromQueryResult(
        query_result, 'last_written_audio_timestamp', 0)
    self.last_written_video_timestamp = GetValueFromQueryResult(
        query_result, 'last_written_video_timestamp', 0)
    self.video_width = GetValueFromQueryResult(query_result, 'video_width', 0)
    self.video_height = GetValueFromQueryResult(query_result, 'video_height', 0)
    self.is_audio_eos_written = GetValueFromQueryResult(query_result,
                                                        'is_audio_eos_written',
                                                        False)
    self.is_video_eos_written = GetValueFromQueryResult(query_result,
                                                        'is_video_eos_written',
                                                        False)
    # If |pipeline_status| is not 0, it indicates there's an error
    # in the pipeline.
    self.pipeline_status = GetValueFromQueryResult(query_result,
                                                   'pipeline_status', 0)
    self.error_message = GetValueFromQueryResult(query_result, 'error_message',
                                                 '')

  def __eq__(self, other):
    if not isinstance(other, self.__class__):
      raise NotImplementedError
    return (self.identifier == other.identifier and
            self.is_started == other.is_started and
            self.is_suspended == other.identifier and
            self.is_stopped == other.identifier and
            self.is_ended == other.is_ended and
            self.player_state == other.player_state and
            self.volume == other.volume and
            self.playback_rate == other.playback_rate and
            self.duration == other.duration and
            self.last_media_time == other.last_media_time and
            self.max_video_capabilities == other.max_video_capabilities and
            self.seek_time == other.seek_time and
            self.first_written_audio_timestamp
            == other.first_written_audio_timestamp and
            self.first_written_video_timestamp
            == other.first_written_video_timestamp and
            self.last_written_audio_timestamp
            == other.last_written_audio_timestamp and
            self.last_written_video_timestamp
            == other.last_written_video_timestamp and
            self.video_width == other.video_width and
            self.video_height == other.video_height and
            self.is_audio_eos_written == other.is_audio_eos_written and
            self.is_video_eos_written == other.is_video_eos_written and
            self.pipeline_status == other.pipeline_status and
            self.error_message == other.error_message)

  def __ne__(self, other):
    return not self.__eq__(other)

  @staticmethod
  def QueryJsCode():
    return """
      const primary_pipeline_keys = h5vcc.cVal.keys().filter(key =>
        key.startsWith("Media.Pipeline.") &&
        key.endsWith("MaxVideoCapabilities") &&
        h5vcc.cVal.getValue(key).length === 0);
      if (primary_pipeline_keys.length == 0) {
        return "null";
      }
      const key_prefix = primary_pipeline_keys[0].slice(0,
                          -".MaxVideoCapabilities".length);
      return {
        identifier: key_prefix.slice("Media.Pipeline.".length),
        is_started: h5vcc.cVal.getValue(key_prefix + '.Started'),
        is_suspended: h5vcc.cVal.getValue(key_prefix + '.Suspended'),
        is_stopped: h5vcc.cVal.getValue(key_prefix + '.Stopped'),
        is_ended: h5vcc.cVal.getValue(key_prefix + '.Ended'),
        player_state: h5vcc.cVal.getValue(key_prefix + '.PlayerState'),
        volume: h5vcc.cVal.getValue(key_prefix + '.Volume'),
        playback_rate: h5vcc.cVal.getValue(key_prefix + '.PlaybackRate'),
        duration: h5vcc.cVal.getValue(key_prefix + '.Duration'),
        last_media_time: h5vcc.cVal.getValue(key_prefix + '.LastMediaTime'),
        max_video_capabilities: h5vcc.cVal.getValue(
                                key_prefix + '.MaxVideoCapabilities'),
        seek_time: h5vcc.cVal.getValue(key_prefix + '.SeekTime'),
        first_written_audio_timestamp:
            h5vcc.cVal.getValue(key_prefix + '.FirstWrittenAudioTimestamp'),
        first_written_video_timestamp:
            h5vcc.cVal.getValue(key_prefix + '.FirstWrittenVideoTimestamp'),
        last_written_audio_timestamp:
            h5vcc.cVal.getValue(key_prefix + '.LastWrittenAudioTimestamp'),
        last_written_video_timestamp:
            h5vcc.cVal.getValue(key_prefix + '.LastWrittenVideoTimestamp'),
        video_width: h5vcc.cVal.getValue(key_prefix + '.VideoWidth'),
        video_height: h5vcc.cVal.getValue(key_prefix + '.VideoHeight'),
        is_audio_eos_written: h5vcc.cVal.getValue(key_prefix +
                              '.IsAudioEOSWritten'),
        is_video_eos_written: h5vcc.cVal.getValue(key_prefix +
                              '.IsVideoEOSWritten'),
        pipeline_status: h5vcc.cVal.getValue(key_prefix + '.PipelineStatus'),
        error_message: h5vcc.cVal.getValue(key_prefix + '.ErrorMessage'),
      };
    """

  def IsPlaying(self):
    return (len(self.identifier) > 0 and self.is_started and
            not self.is_ended and not self.is_suspended and not self.is_stopped)


class MediaSessionPlaybackState(int):
  """Set of media session playback states."""
  NONE = 0
  PAUSED = 1
  PLAYING = 2

  @staticmethod
  def FromString(str):
    if str == 'none':
      return 0
    if str == 'paused':
      return 1
    if str == 'playing':
      return 2
    raise NotImplementedError(
        '"%s" is not a valid media session playback state.' % str)


class MediaSessionState():
  """
    The info we get from HTML MediaSession. We update the states periodically
    via webdriver.
  """

  def __init__(self, query_result=None):
    if not query_result is None and not isinstance(query_result, dict):
      raise NotImplementedError

    metadata = None
    if isinstance(query_result, dict) and query_result.has_key('metadata'):
      metadata = query_result['metadata']

    self.title = GetValueFromQueryResult(metadata, 'title', '')
    self.artist = GetValueFromQueryResult(metadata, 'artist', '')
    self.album = GetValueFromQueryResult(metadata, 'album', '')
    self.playback_state = MediaSessionPlaybackState.FromString(
        GetValueFromQueryResult(query_result, 'playbackState', 'none'))

  def __eq__(self, other):
    if not isinstance(other, self.__class__):
      raise NotImplementedError
    return (self.title == other.title and self.artist == other.artist and
            self.album == other.album and
            self.playback_state == other.playback_state)

  def __ne__(self, other):
    if self.__eq__(other):
      return False
    return True

  @staticmethod
  def QueryJsCode():
    return """
      return {
          metadata: navigator.mediaSession.metadata,
          playbackState: navigator.mediaSession.playbackState,
        };
    """


class VideoElementState():
  """
    The info we get from HTML video element. We update the states periodically
    via webdriver.
  """

  def __init__(self, query_result=None):
    # If there's no player existing, the query return unicode code "null".
    if (not query_result is None and not query_result == u'null' and
        not isinstance(query_result, dict)):
      raise NotImplementedError

    self.has_player = isinstance(query_result, dict)
    self.current_time = GetValueFromQueryResult(query_result, 'current_time',
                                                0.0)
    self.duration = GetValueFromQueryResult(query_result, 'duration', 0.0)
    self.paused = GetValueFromQueryResult(query_result, 'paused', False)
    self.playback_rate = GetValueFromQueryResult(query_result, 'playback_rate',
                                                 0.0)
    self.volume = GetValueFromQueryResult(query_result, 'volume', 0.0)
    self.dropped_video_frames = GetValueFromQueryResult(query_result,
                                                        'dropped_video_frames',
                                                        0)
    self.total_video_frames = GetValueFromQueryResult(query_result,
                                                      'total_video_frames', 0)

  def __eq__(self, other):
    if not isinstance(other, self.__class__):
      raise NotImplementedError
    return (self.has_player == other.has_player and
            self.current_time == other.current_time and
            self.duration == other.duration and self.paused == other.paused and
            self.playback_rate == other.playback_rate and
            self.volume == other.volume and
            self.dropped_video_frames == other.dropped_video_frames and
            self.total_video_frames == other.total_video_frames)

  def __ne__(self, other):
    return not self.__eq__(other)

  @staticmethod
  def QueryJsCode():
    # Looking for a full screen video tag and return that as the primary player.
    return """
      const players = document.querySelectorAll("video");
      if (players && players.length > 0) {
        for (let i = 0; i < players.length; i++) {
          const player = players[i];
          const rect = player.getBoundingClientRect();
          if (rect.width === window.innerWidth ||
              rect.height === window.innerHeight) {
            const quality = player.getVideoPlaybackQuality();
            return {
              current_time: player.currentTime,
              duration: player.duration,
              paused: player.paused,
              playback_rate: player.playbackRate,
              volume: player.volume,
              dropped_video_frames: quality.droppedVideoFrames,
              total_video_frames: quality.totalVideoFrames,
            };
          }
        }
      }
      return "null";
    """


class PlayerStateHandler():
  """The handler of player state."""

  def __init__(self, app):
    self.app = app
    self.state_change_handlers = []
    self.player_state = type('', (), {})()
    self.player_state.pipeline_state = PipelineState()
    self.player_state.media_session_state = MediaSessionState()
    self.player_state.video_element_state = VideoElementState()

    self.app.RegisterPeriodicQuery('MediaPipeline', PipelineState.QueryJsCode(),
                                   self._PipelineStateQueryHandler)
    self.app.RegisterPeriodicQuery('MediaSession',
                                   MediaSessionState.QueryJsCode(),
                                   self._MediaSessionStateQueryHandler)
    self.app.RegisterPeriodicQuery('VideoElement',
                                   VideoElementState.QueryJsCode(),
                                   self._VideoElementStateQueryHandler)

  def AddPlayerStateChangeHandler(self, handler):
    self.state_change_handlers.append(handler)

  def RemovePlayerStateChangeHandler(self, handler):
    self.state_change_handlers.remove(handler)

  def IsPlayerPlaying(self):
    return self.player_state.pipeline_state.IsPlaying()

  def _NotifyHandlersOnStateChanged(self):
    for handler in self.state_change_handlers:
      handler(self, self.player_state)

  def _PipelineStateQueryHandler(self, app, query_name, query_result):
    # Note that the callback is invoked on a different thread.
    del app, query_name  # Unused here.
    new_state = PipelineState(query_result)
    if self.player_state.pipeline_state != new_state:
      self.player_state.pipeline_state = new_state
      self._NotifyHandlersOnStateChanged()

  def _MediaSessionStateQueryHandler(self, app, query_name, query_result):
    # Note that the callback is invoked on a different thread.
    del app, query_name  # Unused here.
    new_state = MediaSessionState(query_result)
    if self.player_state.media_session_state != new_state:
      self.player_state.media_session_state = new_state
      self._NotifyHandlersOnStateChanged()

  def _VideoElementStateQueryHandler(self, app, query_name, query_result):
    # Note that the callback is invoked on a different thread.
    del app, query_name  # Unused here.
    new_state = VideoElementState(query_result)
    if self.player_state.video_element_state != new_state:
      self.player_state.video_element_state = new_state
      self._NotifyHandlersOnStateChanged()


class AdsState(int):
  """Set of ads states. The numeric values are used in ads state query."""
  NONE = 0
  PLAYING = 1
  PLAYING_AND_SKIPPABLE = 2


class TestApp():
  """
    A class encapsulating fundamental functions to control and query data
    from Cobalt application.
  """

  def __init__(self,
               launcher_params,
               url,
               supported_features,
               log_file=None,
               target_params=None,
               success_message=None):
    self.supported_features = supported_features
    self.runner = cobalt_runner.CobaltRunner(
        launcher_params=launcher_params,
        url=url,
        log_handler=self._OnNewLogLine,
        log_file=log_file,
        target_params=target_params,
        success_message=success_message)
    self.log_handlers = []
    self.is_queries_changed = False
    self.periodic_queries = {}
    self.periodic_queries_lock = threading.Lock()
    self.should_exit = threading.Event()
    self.periodic_query_thread = threading.Thread(
        target=self._RunPeriodicQueries)

    self.app_state_handler = ApplicationStateHandler(self)
    self.player_state_handler = PlayerStateHandler(self)

  def __enter__(self):
    try:
      self.runner.__enter__()
    except Exception:
      # Potentially from thread.interrupt_main(). No need to start
      # query thread.
      return self

    self.periodic_query_thread.start()
    return self

  def __exit__(self, *args):
    self.should_exit.set()
    if self.periodic_query_thread.is_alive():
      self.periodic_query_thread.join(THREAD_EXIT_TIMEOUT_SECONDS)
    return self.runner.__exit__(*args)

  def ExecuteScript(self, script):
    try:
      result = self.runner.webdriver.execute_script(script)
    except Exception as e:
      raise RuntimeError('Fail to excute script with error (%s).' % (str(e)))
    return result

  def _OnNewLogLine(self, line):
    # Note that the function is called on cobalt runner reader thread.
    # pass
    self._NotifyHandlersOnNewLogLine(line)

  def _NotifyHandlersOnNewLogLine(self, line):
    for handler in self.log_handlers:
      handler(self, line)

  def ApplicationState(self):
    return self.app_state_handler.app_state

  def PlayerState(self):
    return self.player_state_handler.player_state

  def CurrentMediaTime(self):
    # Convert timestamp from millisecond to second.
    return float(self.PlayerState().pipeline_state.last_media_time) / 1000000.0

  def Suspend(self):
    if not self.supported_features[Features.SUSPEND_AND_RESUME]:
      raise RuntimeError('Suspend is not supported.')
    if not self.runner.launcher_is_running:
      raise RuntimeError('Runner is not running.')
    self.runner.SendSystemSuspend()

  def Resume(self):
    if not self.supported_features[Features.SUSPEND_AND_RESUME]:
      raise RuntimeError('Resume is not supported.')
    if not self.runner.launcher_is_running:
      raise RuntimeError('Runner is not running.')
    self.runner.SendSystemResume()

  def SendKeys(self, keys):
    if not self.supported_features[Features.SEND_KEYS]:
      raise RuntimeError('SendKeys is not supported.')
    if not self.runner.launcher_is_running:
      raise RuntimeError('Runner is not running.')
    self.runner.SendKeys(keys)

  # The handler will receive parameters (TestApp, String).
  def AddLogHandler(self, handler):
    self.log_handlers.append(handler)

  def RemoveLogHandler(self, handler):
    self.log_handlers.remove(handler)

  # The handler will receive parameters (TestApp, ApplicationState).
  def AddAppStateChangeHandler(self, handler):
    self.app_state_handler.AddAppStateChangeHandler(handler)

  def RemoveAppStateChangeHandler(self, handler):
    self.app_state_handler.RemoveAppStateChangeHandler(handler)

  # The handler will receive parameters (TestApp, PlayerState).
  def AddPlayerStateChangeHandler(self, handler):
    self.player_state_handler.AddPlayerStateChangeHandler(handler)

  def RemovePlayerStateChangeHandler(self, handler):
    self.player_state_handler.RemovePlayerStateChangeHandler(handler)

  # The handler will receive parameters (TestApp, String, Dictionary).
  def RegisterPeriodicQuery(self, query_name, query_js_code, result_handler):
    if not isinstance(query_name, str):
      raise RuntimeError('Query name must be a string.')
    if self.periodic_queries.get(query_name):
      raise RuntimeError('Duplicate query name is not allowed.')
    # TODO: Validate js code.
    with self.periodic_queries_lock:
      self.is_queries_changed = True
      self.periodic_queries[query_name] = (query_js_code, result_handler)

  def UnregisterPeriodicQuery(self, query_name):
    if not isinstance(query_name, str):
      raise RuntimeError('Query name must be a string.')
    with self.periodic_queries_lock:
      self.is_queries_changed = True
      self.periodic_queries.pop(query_name)

  def _RunPeriodicQueries(self):
    while True:
      if self.should_exit.is_set():
        break
      # Wait until webdriver client attached to Cobalt.
      if self.runner.test_script_started.is_set():
        with self.periodic_queries_lock:
          local_is_queries_changed = self.is_queries_changed
          local_periodic_queries = self.periodic_queries
          self.is_queries_changed = False
        # Skip if there's no registered query.
        if len(local_periodic_queries) == 0:
          continue
        # Generate javascript code and execute it.
        js_code = self._GeneratePeriodicQueryJsCode(local_is_queries_changed,
                                                    local_periodic_queries)
        result = self.ExecuteScript(js_code)
        for query_name in local_periodic_queries.keys():
          if not result.get(query_name):
            raise RuntimeError(
                'There\'s something wrong in the queries result.')
          local_periodic_queries[query_name][1](self, query_name,
                                                result[query_name])

      time.sleep(PERIODIC_QUERIES_INTERVAL_SECONDS)

  _PERIODIC_QUERIES_JS_CODE = """
    let ret = {};
    for(let key in _media_integration_testing_queries) {
      ret[key] = _media_integration_testing_queries[key]();
    }
    return ret;
  """

  def _GeneratePeriodicQueryJsCode(self, is_queries_changed, periodic_queries):
    js_code = ''
    # Update registered queries at host side.
    if is_queries_changed:
      js_code += '_media_integration_testing_queries = {'
      for query_name in periodic_queries:
        js_code += query_name + ': function() {'
        js_code += periodic_queries[query_name][0]
        js_code += '},'
      js_code += '}'
    # Run queries and collect results.
    js_code += self._PERIODIC_QUERIES_JS_CODE
    return js_code

  # The first input of |state_lambda| is an instance of TestApp.
  def WaitUntilReachState(
      self,
      state_lambda,
      timeout=WAIT_UNTIL_REACH_STATE_DEFAULT_TIMEOUT_SECONDS):
    start_time = time.time()
    while not state_lambda(self) and time.time() - start_time < timeout:
      time.sleep(WAIT_INTERVAL_SECONDS)
    execute_interval = time.time() - start_time
    if execute_interval > timeout:
      raise RuntimeError('WaitUntilReachState timed out after (%f) seconds.' %
                         (execute_interval))

  # The result is an array of overlay header text contents.
  _OVERLAY_QUERY_JS_CODE = """
    let result = [];
    if (document.getElementsByTagName(
         "YTLR-OVERLAY-PANEL-HEADER-RENDERER").length > 0) {
      let childNodes = document.getElementsByTagName(
         "YTLR-OVERLAY-PANEL-HEADER-RENDERER")[0].childNodes;
      for (let i = 0; i < childNodes.length; i++) {
        result.push(childNodes[i].textContent);
      }
    }
    return result;
  """

  def WaitUntilPlayerStart(
      self, timeout=WAIT_UNTIL_REACH_STATE_DEFAULT_TIMEOUT_SECONDS):

    def PlayerStateCheckCallback(app):
      # Check if it's showing account selector page.
      is_showing_account_selector = self.ExecuteScript(
          'return document.getElementsByTagName("YTLR-ACCOUNT-SELECTOR")'
          '.length > 0;')
      if is_showing_account_selector:
        active_element_label_attr = self.ExecuteScript(
            'return document.activeElement.getAttribute("aria-label");')
        if active_element_label_attr != ACCOUNT_SELECTOR_ADD_ACCOUNT_TEXT:
          logging.info('Select an account (%s) to continue the test.',
                       active_element_label_attr)
          self.SendKeys(webdriver_keys.Keys.ENTER)
        else:
          logging.info('Current selected item is "Add acount", move to next'
                       ' item.')
          self.SendKeys(webdriver_keys.Keys.ARROW_RIGHT)
        return False
      # Check if it's showing a playback survey.
      is_showing_skip_button = self.ExecuteScript(
          'return document.getElementsByTagName("YTLR-SKIP-BUTTON-RENDERER")'
          '.length > 0;')
      # When there's a skip button and no running player, it's showing a
      # survey.
      if (is_showing_skip_button and
          not app.player_state_handler.IsPlayerPlaying()):
        self.SendKeys(webdriver_keys.Keys.ENTER)
        logging.info('Send enter key event to skip the survey.')
        return False
      # Check if it's showing an overlay.
      overlay_query_result = self.ExecuteScript(self._OVERLAY_QUERY_JS_CODE)
      if len(overlay_query_result) > 0:
        # Note that if there're playback errors, after close the overlay,
        # the test will end with timeout error.
        self.SendKeys(webdriver_keys.Keys.ENTER)
        logging.info(
            'Send enter key event to close the overlay. Overlay '
            'headers : %r', overlay_query_result)
        return False

      return app.player_state_handler.IsPlayerPlaying()

    self.WaitUntilReachState(PlayerStateCheckCallback, timeout)

  def WaitUntilPlayerDestroyed(
      self, timeout=WAIT_UNTIL_REACH_STATE_DEFAULT_TIMEOUT_SECONDS):
    current_identifier = self.PlayerState().pipeline_state.identifier
    if not current_identifier or len(current_identifier) == 0:
      raise RuntimeError('No existing player when calling'
                         'WaitUntilPlayerDestroyed.')
    self.WaitUntilReachState(
        lambda _app: _app.PlayerState().pipeline_state.identifier !=
        current_identifier, timeout)

  """
    The return values of the query, exact mapping of AdsState defined earlier
    in this file.
  """
  _GET_ADS_STATE_QUERY_JS_CODE = """
    if (document.getElementsByTagName("YTLR-AD-PREVIEW-RENDERER").length > 0) {
      return 1;
    }
    if (document.getElementsByTagName("YTLR-SKIP-BUTTON-RENDERER").length > 0) {
      return 2;
    }
    return 0;
  """

  def GetAdsState(self):
    if not self.runner.test_script_started.is_set():
      raise RuntimeError('Webdriver is not ready yet')
    result = self.ExecuteScript(self._GET_ADS_STATE_QUERY_JS_CODE)
    return AdsState(result)

  def WaitUntilAdsEnd(self, timeout=WAIT_UNTIL_ADS_END_DEFAULT_TIMEOUT_SECONDS):
    start_time = time.time()
    # There may be multiple ads and we need to wait all of them finished.
    while time.time() - start_time < timeout:
      # Wait until the content starts, otherwise the ads state may not be right.
      self.WaitUntilReachState(lambda _app: _app.CurrentMediaTime() > 0,
                               timeout)
      ads_state = self.GetAdsState()
      if ads_state == AdsState.NONE:
        break
      while ads_state != AdsState.NONE and time.time() - start_time < timeout:
        ads_state = self.GetAdsState()
        if ads_state == AdsState.PLAYING_AND_SKIPPABLE:
          self.SendKeys(webdriver_keys.Keys.ENTER)
          logging.info('Send enter key event to skip the ad.')
        time.sleep(WAIT_INTERVAL_SECONDS)

    execute_interval = time.time() - start_time
    if execute_interval > timeout:
      raise RuntimeError(
          'WaitUntilReachState timed out after (%f) seconds, ads_state: (%d).' %
          (execute_interval, ads_state))

  def WaitUntilMediaTimeReached(
      self,
      media_time,
      timeout=WAIT_UNTIL_MEDIA_TIME_REACHED_DEFAULT_TIMEOUT_SECONDS):
    start_media_time = self.CurrentMediaTime()
    if start_media_time > media_time:
      return

    # Wait until playback starts.
    self.WaitUntilReachState(
        lambda _app: _app.PlayerState().pipeline_state.playback_rate > 0,
        timeout)

    duration = self.PlayerState().video_element_state.duration
    if media_time > duration:
      logging.info(
          'Requested media time (%f) is greater than the duration (%f)',
          media_time, duration)
      media_time = duration

    time_to_play = max(media_time - self.CurrentMediaTime(), 0)
    playback_rate = self.PlayerState().pipeline_state.playback_rate
    adjusted_timeout = time_to_play / playback_rate + timeout
    self.WaitUntilReachState(lambda _app: _app.CurrentMediaTime() > media_time,
                             adjusted_timeout)

  def IsMediaTypeSupported(self, mime):
    return self.ExecuteScript('return MediaSource.isTypeSupported("%s");' %
                              (mime))

  def PlayOrPause(self):
    self.SendKeys(AdditionalKeys.MEDIA_PLAY_PAUSE)

  def Fastforward(self):
    # The first fastforward will only bring up the progress bar.
    self.SendKeys(AdditionalKeys.MEDIA_FAST_FORWARD)
    # The second fastforward will forward the playback by 10 seconds.
    self.SendKeys(AdditionalKeys.MEDIA_FAST_FORWARD)
    # Press play button to start the playback.
    self.SendKeys(AdditionalKeys.MEDIA_PLAY_PAUSE)

  def Rewind(self):
    # The first rewind will only bring up the progress bar.
    self.SendKeys(AdditionalKeys.MEDIA_REWIND)
    # The second rewind will rewind the playback by 10 seconds.
    self.SendKeys(AdditionalKeys.MEDIA_REWIND)
    # It needs to press play button to start the playback.
    self.SendKeys(AdditionalKeys.MEDIA_PLAY_PAUSE)

  def PlayPrevious(self):
    self.SendKeys(AdditionalKeys.MEDIA_PREV_TRACK)

  def PlayNext(self):
    self.SendKeys(AdditionalKeys.MEDIA_NEXT_TRACK)

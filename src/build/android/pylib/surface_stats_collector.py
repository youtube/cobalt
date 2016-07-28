# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import Queue
import datetime
import logging
import re
import threading

from pylib import perf_tests_helper


# Log marker containing SurfaceTexture timestamps.
_SURFACE_TEXTURE_TIMESTAMPS_MESSAGE = 'SurfaceTexture update timestamps'
_SURFACE_TEXTURE_TIMESTAMP_RE = '\d+'


class SurfaceStatsCollector(object):
  """Collects surface stats for a window from the output of SurfaceFlinger.

  Args:
    adb: the adb coonection to use.
    window_package: Package name of the window.
    window_activity: Activity name of the window.
  """
  def __init__(self, adb, window_package, window_activity, trace_tag):
    self._adb = adb
    self._window_package = window_package
    self._window_activity = window_activity
    self._trace_tag = trace_tag
    self._collector_thread = None
    self._use_legacy_method = False
    self._surface_before = None
    self._get_data_event = None
    self._data_queue = None
    self._stop_event = None

  def __enter__(self):
    assert not self._collector_thread

    if self._ClearSurfaceFlingerLatencyData():
      self._get_data_event = threading.Event()
      self._stop_event = threading.Event()
      self._data_queue = Queue.Queue()
      self._collector_thread = threading.Thread(target=self._CollectorThread)
      self._collector_thread.start()
    else:
      self._use_legacy_method = True
      self._surface_before = self._GetSurfaceStatsLegacy()

  def __exit__(self, *args):
    self._PrintPerfResults()
    if self._collector_thread:
      self._stop_event.set()
      self._collector_thread.join()
      self._collector_thread = None

  def _PrintPerfResults(self):
    if self._use_legacy_method:
      surface_after = self._GetSurfaceStatsLegacy()
      td = surface_after['timestamp'] - self._surface_before['timestamp']
      seconds = td.seconds + td.microseconds / 1e6
      frame_count = (surface_after['page_flip_count'] -
                     self._surface_before['page_flip_count'])
    else:
      assert self._collector_thread
      (seconds, latencies) = self._GetDataFromThread()
      if not seconds or not len(latencies):
        logging.warning('Surface stat data is empty')
        return

      frame_count = len(latencies)
      jitter_count = 0
      last_latency = latencies[0]
      for latency in latencies[1:]:
        if latency > last_latency:
          jitter_count = jitter_count + 1
        last_latency = latency

      perf_tests_helper.PrintPerfResult(
          'surface_latencies', 'surface_latencies' + self._trace_tag,
          latencies, '')
      perf_tests_helper.PrintPerfResult(
          'peak_jitter', 'peak_jitter' + self._trace_tag, [max(latencies)], '')
      perf_tests_helper.PrintPerfResult(
          'jitter_percent', 'jitter_percent' + self._trace_tag,
          [jitter_count * 100.0 / frame_count], 'percent')

    print 'SurfaceMonitorTime: %fsecs' % seconds
    perf_tests_helper.PrintPerfResult(
        'avg_surface_fps', 'avg_surface_fps' + self._trace_tag,
        [int(round(frame_count / seconds))], 'fps')

  def _CollectorThread(self):
    last_timestamp = 0
    first_timestamp = 0
    latencies = []

    while not self._stop_event.is_set():
      self._get_data_event.wait(1)
      try:
        (t, last_timestamp) = self._GetSurfaceFlingerLatencyData(last_timestamp,
                                                                 latencies)
        if not first_timestamp:
          first_timestamp = t

        if self._get_data_event.is_set():
          self._get_data_event.clear()
          self._data_queue.put(((last_timestamp - first_timestamp) / 1e9,
                                latencies))
          latencies = []
          first_timestamp = 0
      except Exception as e:
        # On any error, before aborting, put the exception into _data_queue to
        # prevent the main thread from waiting at _data_queue.get() infinitely.
        self._data_queue.put(e)
        raise

  def _GetDataFromThread(self):
    self._get_data_event.set()
    ret = self._data_queue.get()
    if isinstance(ret, Exception):
      raise ret
    return ret

  def _ClearSurfaceFlingerLatencyData(self):
    """Clears the SurfaceFlinger latency data.

    Returns:
      True if SurfaceFlinger latency is supported by the device, otherwise
      False.
    """
    # The command returns nothing if it is supported, otherwise returns many
    # lines of result just like 'dumpsys SurfaceFlinger'.
    results = self._adb.RunShellCommand(
        'dumpsys SurfaceFlinger --latency-clear %s/%s' %
        (self._window_package, self._window_activity))
    return not len(results)

  def _GetSurfaceFlingerLatencyData(self, previous_timestamp, latencies):
    """Returns collected SurfaceFlinger latency data.

    Args:
      previous_timestamp: The timestamp returned from the previous call or 0.
          Only data after this timestamp will be returned.
      latencies: A list to receive latency data. The latencies are integers
          each of which is the number of refresh periods of each frame.

    Returns:
      A tuple containing:
      - The timestamp of the beginning of the first frame (ns),
      - The timestamp of the end of the last frame (ns).

    Raises:
      Exception if failed to run the SurfaceFlinger command or SurfaceFlinger
          returned invalid result.
    """
    # adb shell dumpsys SurfaceFlinger --latency <window name>
    # prints some information about the last 128 frames displayed in
    # that window.
    # The data returned looks like this:
    # 16954612
    # 7657467895508   7657482691352   7657493499756
    # 7657484466553   7657499645964   7657511077881
    # 7657500793457   7657516600576   7657527404785
    # (...)
    #
    # The first line is the refresh period (here 16.95 ms), it is followed
    # by 128 lines w/ 3 timestamps in nanosecond each:
    # A) when the app started to draw
    # B) the vsync immediately preceding SF submitting the frame to the h/w
    # C) timestamp immediately after SF submitted that frame to the h/w
    #
    # The difference between the 1st and 3rd timestamp is the frame-latency.
    # An interesting data is when the frame latency crosses a refresh period
    # boundary, this can be calculated this way:
    #
    # ceil((C - A) / refresh-period)
    #
    # (each time the number above changes, we have a "jank").
    # If this happens a lot during an animation, the animation appears
    # janky, even if it runs at 60 fps in average.
    results = self._adb.RunShellCommand(
        'dumpsys SurfaceFlinger --latency %s/%s' %
        (self._window_package, self._window_activity), log_result=True)
    assert len(results)

    refresh_period = int(results[0])
    last_timestamp = previous_timestamp
    first_timestamp = 0
    for line in results[1:]:
      fields = line.split()
      if len(fields) == 3:
        timestamp = long(fields[0])
        last_timestamp = long(fields[2])
        if (timestamp > previous_timestamp):
          if not first_timestamp:
            first_timestamp = timestamp
          # This is integral equivalent of ceil((C-A) / refresh-period)
          latency_ns = int(last_timestamp - timestamp)
          latencies.append((latency_ns + refresh_period - 1) / refresh_period)
    return (first_timestamp, last_timestamp)

  def _GetSurfaceStatsLegacy(self):
    """Legacy method (before JellyBean), returns the current Surface index
       and timestamp.

    Calculate FPS by measuring the difference of Surface index returned by
    SurfaceFlinger in a period of time.

    Returns:
      Dict of {page_flip_count (or 0 if there was an error), timestamp}.
    """
    results = self._adb.RunShellCommand('service call SurfaceFlinger 1013')
    assert len(results) == 1
    match = re.search('^Result: Parcel\((\w+)', results[0])
    cur_surface = 0
    if match:
      try:
        cur_surface = int(match.group(1), 16)
      except Exception:
        logging.error('Failed to parse current surface from ' + match.group(1))
    else:
      logging.warning('Failed to call SurfaceFlinger surface ' + results[0])
    return {
        'page_flip_count': cur_surface,
        'timestamp': datetime.datetime.now(),
    }

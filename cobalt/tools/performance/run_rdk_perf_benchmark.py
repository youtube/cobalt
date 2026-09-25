#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Automated YouTube on TV Critical User Journey (CUJ) Benchmark for RDK.

Drives deterministic YouTube TV Critical User Journeys (CUJs) over Chrome
DevTools Protocol (CDP) WebSocket telemetry on physical RDK devices:

Supported Critical User Journeys (CUJs):
  1. 'watch' (Steady Video Playback CUJ):
     - Launches YouTube on TV with the reference certification video
       (v=1La4QzGeaaQ).
     - Detects and automatically skips pre-roll ads via CDP input coordinates.
     - Maintains steady-state continuous video playback.
     - Evaluates hardware media decoder QoS (droppedVideoFrames /
       totalVideoFrames), display compositor VSync RAF cadence, and background
       task duration.

  2. 'watch_browse' (Watch & Recommendations Browse CUJ):
     - Stabilizes initial playback for 15 seconds.
     - Shifts focus off the media player scrubber into recommendations
       (3x ArrowDown).
     - Smoothly scrolls down the recommendation shelf (every 2 seconds)
       while video continues playing in the background.
     - Evaluates concurrent video decoding, GPU tile rasterization, UI
       compositor smoothness, W3C long tasks (>=50ms/150ms hitches), and
       style/layout reflow under user interaction.
"""

import argparse
import csv
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request

CUJ_WATCH = 'watch'
CUJ_WATCH_BROWSE = 'watch_browse'
DEFAULT_DEEPLINK = 'v=1La4QzGeaaQ'

CUJ_DESCRIPTIONS = {
    CUJ_WATCH: ('YouTube on TV: Steady Video Playback CUJ '
                '(Continuous Hardware Media Decoding)'),
    CUJ_WATCH_BROWSE:
        ('YouTube on TV: Watch & Recommendations Browse CUJ '
         '(Concurrent Video Playback + D-Pad Recommendations Scrolling)'),
}


def find_repo_root():
  """Dynamically locates the root of the Cobalt source checkout."""
  curr = os.path.abspath(os.path.dirname(__file__))
  while curr and curr != os.path.dirname(curr):
    if os.path.exists(os.path.join(curr, 'cobalt')) and os.path.exists(
        os.path.join(curr, 'starboard')):
      return curr
    curr = os.path.dirname(curr)
  return os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))


REPO_ROOT = find_repo_root()
if not os.path.exists(os.path.join(REPO_ROOT, 'cobalt')):
  raise RuntimeError(f'Cobalt directory not found at {REPO_ROOT}')

try:
  import websocket
except ImportError:
  websocket_dir = os.path.join(
      REPO_ROOT, 'third_party/catapult/telemetry/third_party/websocket-client')
  if os.path.exists(websocket_dir) and websocket_dir not in sys.path:
    sys.path.insert(0, websocket_dir)
  import websocket  # pylint: disable=wrong-import-position


def run_cmd(cmd, check=True):
  """Executes command and prints stdout."""
  cmd_str = ' '.join(cmd) if isinstance(cmd, list) else cmd
  print(f'>>> Executing: {cmd_str}')
  return subprocess.run(cmd, shell=isinstance(cmd, str), check=check)


def wait_for_cdp_url(port=9222, max_retries=30, retry_delay_s=2.0):
  """Polls http://localhost:<port>/json until YouTube TV target is found."""
  print(f'Waiting for CDP endpoint on localhost:{port}...')
  url = f'http://127.0.0.1:{port}/json'
  for _ in range(max_retries):
    try:
      with urllib.request.urlopen(url, timeout=3) as resp:
        if resp.status == 200:
          targets = json.loads(resp.read().decode('utf-8'))
          for target in targets:
            if (target.get('type') == 'page' and
                'youtube' in (target.get('title') or '').lower() and
                target.get('webSocketDebuggerUrl')):
              ws_url = target['webSocketDebuggerUrl']
              target_title = target.get('title')
              print(
                  f'Connected to YouTube TV target: {target_title} ({ws_url})')
              return ws_url
    except (urllib.error.URLError, TimeoutError, OSError, json.JSONDecodeError):
      pass
    time.sleep(retry_delay_s)
  raise TimeoutError(
      f'Failed to connect to CDP on port {port} after {max_retries} attempts.')


class CdpBenchmarkSession:
  """Manages WebSocket telemetry streaming and simulated workload injection.

  Why:
    Provides direct in-engine WebSocket telemetry collection (Blink Scheduler
    task duration, W3C Long Tasks, VSync RAF cadence, hardware video playback
    QoS, and memory footprint) for physical RDK devices.

  Lifetime and Ownership:
    Instantiated at the start of a benchmark run and closed/destroyed
    immediately after the run loop completes. Owned by the main execution
    thread.

  Threading Model:
    This class is thread-affine and must be used exclusively from the main
    thread (not thread-safe).
  """

  def __init__(self, ws_url_getter, output_csv, max_retries=15):
    self.ws_url_getter = ws_url_getter
    self.output_csv = output_csv
    self.ws = None
    self._logged_events = set()

    # Retry connection in case DevTools is still finishing page attachment
    ws_url = self.ws_url_getter()
    last_err = None
    for _ in range(max_retries):
      try:
        self.ws = websocket.create_connection(ws_url, timeout=10)
        break
      except (websocket.WebSocketException, OSError, TimeoutError) as e:
        last_err = e
        time.sleep(2.0)

    if not self.ws:
      raise ConnectionError(
          f'Failed to establish WebSocket after {max_retries} attempts: '
          f'{last_err}')

    self.msg_id = 1
    self._send('Performance.enable', {})
    self._inject_performance_probes()
    self.records = []

  def _send(self, method, params):
    self.msg_id += 1
    payload = json.dumps({
        'id': self.msg_id,
        'method': method,
        'params': params,
    })
    try:
      self.ws.send(payload)
      raw = self.ws.recv()
      return json.loads(raw)
    except (websocket.WebSocketException, OSError, json.JSONDecodeError) as e:
      print(f'CDP communication error: {e}. Reconnecting...')
      self._reconnect()
      self.ws.send(payload)
      raw = self.ws.recv()
      return json.loads(raw)

  def _reconnect(self):
    """Re-establishes connection to the active page target."""
    ws_url = self.ws_url_getter()
    last_err = None
    for _ in range(15):
      try:
        self.ws = websocket.create_connection(ws_url, timeout=10)
        self.ws.send(
            json.dumps({
                'id': 1,
                'method': 'Performance.enable',
                'params': {},
            }))
        self.ws.recv()
        self._inject_performance_probes()
        print('Reconnected to Cobalt DevTools target successfully.')
        return
      except (websocket.WebSocketException, OSError, TimeoutError) as e:
        last_err = e
        time.sleep(2.0)
    raise ConnectionError(
        f'Reconnection to Cobalt DevTools target failed after 15 attempts: '
        f'{last_err}')

  def _inject_performance_probes(self):
    """Injects high-res RAF probe and LongTask observer into the page."""
    probe_script = """
    (() => {
      if (window.__rdkPerfProbesInjected) return;
      window.__rdkPerfProbesInjected = true;

      // 1. Continuous high-resolution Compositor RAF Frame Counter
      window.__rdkRafData = {
        totalVsyncs: 0,
        janks: 0,
        lastTimestamp: performance.now(),
        dips: 0
      };

      const onVsync = (now) => {
        const delta = now - window.__rdkRafData.lastTimestamp;
        window.__rdkRafData.totalVsyncs++;
        // If frame interval > 1.5 vsyncs (~25ms at 60Hz), record jank/dip
        if (delta > 25.0 && window.__rdkRafData.totalVsyncs > 10) {
          window.__rdkRafData.janks++;
        }
        window.__rdkRafData.lastTimestamp = now;
        requestAnimationFrame(onVsync);
      };
      requestAnimationFrame(onVsync);

      // 2. W3C Long Tasks Observer (stalls >= 50ms and severe freezes >= 150ms)
      window.__rdkLongTasks = { count50ms: 0, count150ms: 0, totalDurationMs: 0 };
      try {
        const obs = new PerformanceObserver((list) => {
          for (const entry of list.getEntries()) {
            window.__rdkLongTasks.count50ms++;
            if (entry.duration >= 150) {
              window.__rdkLongTasks.count150ms++;
            }
            window.__rdkLongTasks.totalDurationMs += entry.duration;
          }
        });
        obs.observe({ entryTypes: ['longtask'] });
      } catch (e) {}
    })()
    """
    try:
      self._send('Runtime.evaluate', {'expression': probe_script})
    except (websocket.WebSocketException, OSError, json.JSONDecodeError) as e:
      print(f'Warning: Could not inject performance probes: {e}')

  def dispatch_key(self, key_code, key_identifier):
    """Sends a keydown and keyup pair over CDP Input domain."""
    try:
      self._send(
          'Input.dispatchKeyEvent', {
              'type': 'keyDown',
              'windowsVirtualKeyCode': key_code,
              'code': key_identifier,
              'key': key_identifier,
          })
      time.sleep(0.1)
      self._send(
          'Input.dispatchKeyEvent', {
              'type': 'keyUp',
              'windowsVirtualKeyCode': key_code,
              'code': key_identifier,
              'key': key_identifier,
          })
    except (websocket.WebSocketException, OSError, json.JSONDecodeError) as e:
      print(f'Warning: Key dispatch failed: {e}')

  def dispatch_mouse_click(self, x, y):
    """Dispatches a left mouse click at given viewport coordinates."""
    try:
      self._send(
          'Input.dispatchMouseEvent', {
              'type': 'mousePressed',
              'x': int(x),
              'y': int(y),
              'button': 'left',
              'clickCount': 1,
          })
      time.sleep(0.05)
      self._send(
          'Input.dispatchMouseEvent', {
              'type': 'mouseReleased',
              'x': int(x),
              'y': int(y),
              'button': 'left',
              'clickCount': 1,
          })
    except (websocket.WebSocketException, OSError, json.JSONDecodeError) as e:
      print(f'Warning: Mouse click failed: {e}')

  def check_player_status(self, check_ads=True):
    """Queries DOM for ad state, skip button, and video playback status."""
    check_ads_js = 'true' if check_ads else 'false'
    expr = f"""
    (() => {{
      let skipCoords = null;
      let skipClicked = false;
      let isAd = false;
      let adCountdown = null;

      if ({check_ads_js}) {{
        const adEl = document.querySelector(
            '.ad-interrupting, .ad-showing, .video-ads, [class*="ad-created"]');
        const previewEl = document.querySelector(
            'ytlr-ad-skip-or-preview, ytlr-skip-button-renderer');
        const isPreviewVisible = previewEl &&
            previewEl.getBoundingClientRect().width > 0;
        isAd = !!(adEl || isPreviewVisible);

        // Find candidate skip buttons
        const sel = [
            'ytlr-skip-button-renderer button',
            'ytlr-skip-button-renderer',
            'ytlr-ad-skip-or-preview button',
            'ytlr-ad-skip-or-preview',
            '.ytlr-ad-skip-or-preview',
            'button.ytp-ad-skip-button',
            '.ytp-ad-skip-button-modern',
            '.ytp-ad-skip-button',
            '.ytp-ad-skip-button-slot button',
            '[class*="skip-button"]',
            '[class*="ad-skip"]',
            '.videoAdUiSkipButton',
            'button[aria-label*="Skip" i]',
            '[role="button"][aria-label*="Skip" i]',
        ].join(', ');

        const candidates = Array.from(document.querySelectorAll(sel));
        for (const btn of candidates) {{
          const ariaLabel = (
              btn.getAttribute('aria-label') || '').toLowerCase();
          const text = (
              btn.innerText || btn.textContent || '').trim().toLowerCase();

          // Reject scrubber/storyboard elements
          if (ariaLabel.includes('skip back') ||
              ariaLabel.includes('skip forward') ||
              text.includes('skip back') ||
              text.includes('skip forward')) {{
            continue;
          }}

          isAd = true;

          // Check if disabled or counting down
          const isAriaDisabled = (
              btn.getAttribute('aria-disabled') === 'true');
          const isNativeDisabled = !!btn.disabled;
          const isCountingDown = (
              /\\bin\\s+\\d+/i.test(text) || /\\b\\d+\\s*s\\b/i.test(text));

          const r = btn.getBoundingClientRect();
          const s = window.getComputedStyle(btn);
          const isVisible = (r.width > 0 && r.height > 0 &&
                             s.display !== 'none' &&
                             s.visibility !== 'hidden' &&
                             s.opacity !== '0');

          if (!isVisible) {{
            continue;
          }}

          if (isAriaDisabled || isNativeDisabled || isCountingDown) {{
            adCountdown = text || 'countdown';
            continue;
          }}

          // Active, clickable skip button found!
          skipCoords = {{x: r.x + r.width / 2, y: r.y + r.height / 2}};
          try {{
            btn.focus();
            btn.click();
            const innerBtn = btn.querySelector('button');
            if (innerBtn) {{
              innerBtn.click();
            }}
            skipClicked = true;
          }} catch (err) {{}}
          break;
        }}
      }}

      // Properly declare video element reference
      const video = document.querySelector('video');
      const isVideoPlaying = !!(video && !video.paused &&
                                !video.ended && video.readyState >= 2);
      let videoData = null;
      if (video) {{
        let q = null;
        try {{
          if (typeof video.getVideoPlaybackQuality === 'function') {{
            q = video.getVideoPlaybackQuality();
          }}
        }} catch (e) {{}}
        videoData = {{
          totalVideoFrames: q ? q.totalVideoFrames :
              (video.webkitDecodedFrameCount || 0),
          droppedVideoFrames: q ? q.droppedVideoFrames :
              (video.webkitDroppedFrameCount || 0),
          corruptedVideoFrames: q ? q.corruptedVideoFrames : 0,
        }};
      }}

      // Harvest RAF and Long Task data
      const rafData = window.__rdkRafData || {{ totalVsyncs: 0, janks: 0 }};
      const longTasks = window.__rdkLongTasks || {{
        count50ms: 0,
        count150ms: 0,
        totalDurationMs: 0
      }};

      return JSON.stringify({{
        isAd: isAd,
        skipCoords: skipCoords,
        skipClicked: skipClicked,
        adCountdown: adCountdown,
        isVideoPlaying: isVideoPlaying,
        currentTime: video ? video.currentTime : 0,
        videoDuration: video ? video.duration : null,
        videoData: videoData,
        totalVsyncs: rafData.totalVsyncs,
        rafJanks: rafData.janks,
        longTasks50ms: longTasks.count50ms,
        longTasks150ms: longTasks.count150ms,
        longTasksTotalMs: longTasks.totalDurationMs
      }});
    }})()
    """
    try:
      resp = self._send('Runtime.evaluate', {'expression': expr})
      val = resp.get('result', {}).get('result', {}).get('value')
      if val:
        return json.loads(val)
    except (websocket.WebSocketException, OSError, json.JSONDecodeError):
      pass
    return {
        'isAd': False,
        'skipCoords': None,
        'skipClicked': False,
        'adCountdown': None,
        'isVideoPlaying': False,
        'currentTime': 0,
        'videoDuration': None,
        'videoData': None,
        'totalVsyncs': 0,
        'rafJanks': 0,
        'longTasks50ms': 0,
        'longTasks150ms': 0,
        'longTasksTotalMs': 0,
    }

  def sample(self, status=None):
    """Samples current performance metrics and private memory footprint."""
    perf_resp = self._send('Performance.getMetrics', {})
    metrics = {
        m['name']: m['value']
        for m in perf_resp.get('result', {}).get('metrics', [])
    }

    # Query Chromium Compositor Dropped Frames UMA histogram if supported
    compositor_dropped_pct = 0.0
    try:
      hist_resp = self._send(
          'Browser.getHistogram',
          {'name': 'Graphics.Smoothness.PercentDroppedFrames3.AllSequences'},
      )
      hist = hist_resp.get('result', {}).get('histogram', {})
      # If histogram exists, extract mean or sum
      if hist.get('count', 0) > 0:
        compositor_dropped_pct = hist.get('sum', 0) / float(hist['count'])
    except (websocket.WebSocketException, OSError, json.JSONDecodeError):
      pass

    # Extract Memory metrics directly from Performance.getMetrics
    footprint_bytes = metrics.get('Memory.Browser.PrivateMemoryFootprint.Live',
                                  0)
    if not footprint_bytes:
      footprint_bytes = metrics.get('Memory.Browser.ResidentSet.Live', 0)

    vid_data = (status or {}).get('videoData') or {}

    now = time.time()
    record = {
        'timestamp': now,
        'vsync_frames': (status or {}).get('totalVsyncs', 0),
        'compositor_janks': (status or {}).get('rafJanks', 0),
        'compositor_dropped_pct': compositor_dropped_pct,
        'long_tasks_50ms': (status or {}).get('longTasks50ms', 0),
        'long_tasks_150ms': (status or {}).get('longTasks150ms', 0),
        'long_tasks_total_ms': (status or {}).get('longTasksTotalMs', 0.0),
        'total_video_frames': vid_data.get('totalVideoFrames', 0),
        'dropped_video_frames': vid_data.get('droppedVideoFrames', 0),
        'corrupted_video_frames': vid_data.get('corruptedVideoFrames', 0),
        'task_duration_sec': metrics.get('TaskDuration', 0.0),
        'script_duration_sec': metrics.get('ScriptDuration', 0.0),
        'layout_duration_sec': metrics.get('LayoutDuration', 0.0),
        'recalc_style_duration_sec': metrics.get('RecalcStyleDuration', 0.0),
        'js_heap_used_bytes': metrics.get('JSHeapUsedSize', 0),
        'js_heap_total_bytes': metrics.get('JSHeapTotalSize', 0),
        'private_memory_footprint_kb': footprint_bytes / 1024.0,
    }
    self.records.append(record)
    return record

  def run_loop(self, duration_sec=180, interval_sec=1.0, workload=CUJ_WATCH):
    """Main polling loop with deterministic playback and browsing workload."""
    start_time = time.time()
    cuj_desc = CUJ_DESCRIPTIONS.get(workload, workload)
    print(f'Starting automated {duration_sec}s benchmark recording...')
    print(f'Active CUJ: {cuj_desc}')

    shelf_entered = False
    content_play_start = None
    last_skip_time = 0.0
    is_ad = False

    while (time.time() - start_time) < duration_sec:
      elapsed = time.time() - start_time
      # Actively check for ads during first 90 seconds or if an ad was active
      should_check_ads = (elapsed < 90.0) or is_ad
      status = self.check_player_status(check_ads=should_check_ads)
      rec = self.sample(status=status)

      is_ad = status.get('isAd', False)
      skip_coords = status.get('skipCoords')
      skip_clicked = status.get('skipClicked', False)
      ad_countdown = status.get('adCountdown')
      is_video_playing = status.get('isVideoPlaying', False)
      video_duration = status.get('videoDuration')

      # 1. Skip button actions
      if (skip_clicked or
          skip_coords) and (time.time() - last_skip_time) >= 1.5:
        coords_str = f' at {skip_coords}' if skip_coords else ''
        print(f'[{elapsed:.1f}s][CUJ: Ad Skip] Active skip button detected! '
              f'Dispatched DOM click + Enter key + mouse click{coords_str}.')
        self.dispatch_key(13, 'Enter')
        if skip_coords:
          self.dispatch_mouse_click(skip_coords['x'], skip_coords['y'])
        last_skip_time = time.time()
      elif is_ad and ad_countdown and int(elapsed) % 2 == 0:
        event_key = f'ad_log_{int(elapsed)}'
        if event_key not in self._logged_events:
          self._logged_events.add(event_key)
          print(f'[{elapsed:.1f}s][CUJ: Ad Wait] Ad active ({ad_countdown}). '
                'Waiting for skip button to become interactive...')

      # 2. Track when main video content starts playing
      is_main_video = (video_duration is None or
                       video_duration > 180.0) and not is_ad
      if is_main_video and is_video_playing:
        if content_play_start is None:
          content_play_start = time.time()
          c_time = status.get('currentTime', 0)
          print(f'[{elapsed:.1f}s][CUJ: Steady Playback] Video started '
                f'(currentTime={c_time:.1f}s). Stabilizing playback...')

      # 3. If workload includes browsing, transition into recommendations shelf
      if workload == CUJ_WATCH_BROWSE:
        if (content_play_start and not shelf_entered and
            (time.time() - content_play_start) >= 15):
          c_elapsed = time.time() - content_play_start
          print(f'[{elapsed:.1f}s][CUJ: Browse Transition] Video played for '
                f'{c_elapsed:.1f}s. Moving focus to recommendations...')
          for _ in range(3):
            self.dispatch_key(40, 'ArrowDown')
            time.sleep(0.8)
          shelf_entered = True

        # 4. In recommendation shelf, browse down smoothly every 2 seconds
        if shelf_entered and elapsed <= (duration_sec - 5):
          event_key = f'down_at_{int(elapsed)}'
          if int(elapsed) % 2 == 0 and event_key not in self._logged_events:
            self._logged_events.add(event_key)
            print(f'[{elapsed:.1f}s][CUJ: Browse Scroll] Scrolling down in '
                  'recommendations...')
            self.dispatch_key(40, 'ArrowDown')

      event_key = f'logged_at_{int(elapsed)}'
      if int(elapsed) % 10 == 0 and event_key not in self._logged_events:
        self._logged_events.add(event_key)
        ad_str = ' (Ad active)' if is_ad else ''
        vid_frames = rec.get('total_video_frames', 0)
        vid_drops = rec.get('dropped_video_frames', 0)
        vsyncs = rec.get('vsync_frames', 0)
        janks = rec.get('compositor_janks', 0)
        t_dur = rec['task_duration_sec']
        ram_mb = rec['private_memory_footprint_kb'] / 1024.0
        print(f'[{elapsed:.1f}s / {duration_sec}s]{ad_str} '
              f'Vsyncs: {vsyncs} (Janks: {janks}) | '
              f'TaskDuration: {t_dur:.2f}s | '
              f'Video: {vid_frames} (Drops: {vid_drops}) | '
              f'PrivateRAM: {ram_mb:.1f} MB')

      time.sleep(interval_sec)

    self.save_csv()

  def save_csv(self):
    """Writes gathered timeseries data to CSV file."""
    if not self.records:
      return
    output_dir = os.path.dirname(os.path.abspath(self.output_csv))
    if output_dir:
      os.makedirs(output_dir, exist_ok=True)
    fieldnames = list(self.records[0].keys())
    with open(self.output_csv, 'w', newline='', encoding='utf-8') as f:
      writer = csv.DictWriter(f, fieldnames=fieldnames)
      writer.writeheader()
      writer.writerows(self.records)
    print(f'Benchmark data successfully written to {self.output_csv}')

  def close(self):
    try:
      self.ws.close()
    except (websocket.WebSocketException, OSError):
      pass


def main():
  parser = argparse.ArgumentParser(
      description=(
          'Automated YouTube on TV Critical User Journey (CUJ) Benchmark '
          'for RDK devices over Chrome DevTools Protocol (CDP).'),
      formatter_class=argparse.RawDescriptionHelpFormatter,
  )
  parser.add_argument(
      '--enable-features',
      default='',
      help='Comma-separated features to enable.',
  )
  parser.add_argument(
      '--disable-features',
      default='',
      help='Comma-separated features to disable.',
  )
  parser.add_argument(
      '--extra-params',
      nargs='*',
      default=[],
      help='Additional command-line parameters to pass to Cobalt (e.g. --v=1).',
  )
  parser.add_argument(
      '--duration-sec',
      type=int,
      default=180,
      help='Duration of test in seconds (default: 180s).',
  )
  parser.add_argument(
      '--workload',
      choices=[CUJ_WATCH, CUJ_WATCH_BROWSE],
      default=CUJ_WATCH,
      help=(
          f"Benchmarking CUJ workload: '{CUJ_WATCH}' (steady video playback) "
          f"or '{CUJ_WATCH_BROWSE}' (steady playback + recommendation browse)."
      ),
  )
  parser.add_argument(
      '--deeplink',
      default=DEFAULT_DEEPLINK,
      help=(
          f'Deeplink parameter for fixed CUJ (defaults to official YouTube TV '
          f'certification stream: {DEFAULT_DEEPLINK}).'),
  )
  parser.add_argument(
      '--skip-deploy',
      action='store_true',
      help='Skip recompiling/pushing if binaries are already on device.',
  )
  parser.add_argument(
      '--skip-launch',
      action='store_true',
      help='Connect to already running Cobalt instance without restarting it.',
  )
  parser.add_argument(
      '--output', required=True, help='Output path for metrics CSV.')
  args = parser.parse_args()

  deploy_script = os.path.join(
      REPO_ROOT,
      'starboard/contrib/rdk/src/third_party/starboard/rdk/arm/scripts',
      'deploy_rdk.py',
  )

  cuj_desc = CUJ_DESCRIPTIONS.get(args.workload, args.workload)

  if not args.skip_launch:
    # Formulate deploy_rdk command
    deploy_cmd = [
        'python3',
        deploy_script,
        '--config',
        'qa',
        '--run',
        '--deeplink',
        args.deeplink,
    ]

    if not args.skip_deploy:
      deploy_cmd.append('--force-deploy')
    else:
      deploy_cmd.extend(['--skip-build', '--skip-deploy'])

    # Forward explicit params
    deploy_params = []
    if args.enable_features:
      deploy_params.append(f'--enable-features={args.enable_features}')
    if args.disable_features:
      deploy_params.append(f'--disable-features={args.disable_features}')
    if args.extra_params:
      deploy_params.extend(args.extra_params)

    if deploy_params:
      deploy_cmd.append('--param')
      deploy_cmd.extend(deploy_params)

    print('=== Starting Benchmark Run on RDK ===')
    print(f'Target CUJ: {cuj_desc}')
    deploy_cmd_str = ' '.join(deploy_cmd)
    print(f'Deploy command: {deploy_cmd_str}')
    # 1. Launch Cobalt on RDK
    run_cmd(deploy_cmd)

  # 2. Wait for CDP port forwarding to become active and sample metrics
  session = None
  try:
    session = CdpBenchmarkSession(lambda: wait_for_cdp_url(port=9222),
                                  args.output)
    session.run_loop(
        duration_sec=args.duration_sec,
        interval_sec=1.0,
        workload=args.workload)
  finally:
    if session:
      session.close()
    # 3. Clean up device session if we launched it
    if not args.skip_launch:
      print('=== Benchmark run complete. Cleaning up RDK session ===')
      run_cmd(['python3', deploy_script, '--reset'])


if __name__ == '__main__':
  main()

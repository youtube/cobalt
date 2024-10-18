"""Test script to measure app starts on Android, with webdriver."""

import argparse
import collections
import contextlib
import csv
import logging
import statistics
import subprocess
import time
import urllib3.exceptions

try:
  from selenium import webdriver
  from selenium.webdriver.remote import remote_connection
except ImportError as e:
  raise RuntimeError('Please install `selenium`, pip install selenium'
                     ' OR apt-get install python3-selenium') from e

# pylint: disable=protected-access

log = logging.getLogger(__name__)

# android_devtools =  'http://127.0.0.1:9222'
android_webdriver = 'http://127.0.0.1:4444'

# youtube/web/core/logging/csi/csi_tick.ts
# +
# video/youtube/web/living_room/core/logging/csi.ts
tick_names = collections.OrderedDict({
    '_start': 'start',
    'hs': 'head_start',
    'ld_s': 'load_start',
    'base_js_r': 'base_js_ready',
    'br_s': 'browse_request',
    'tvcs': 'tv_config_start',
    'main_js_rq': 'main_js_requested',
    'tvce': 'tv_config_end',
    'main_js_r': 'main_js_ready',
    'srt': 'server_response',
    'nreqs': 'navication_request_start',
    'nress': 'navigation_response_start',
    'css_e': 'css_load_end',
    'nrese': 'navigation_response_end',
    'br_r': 'browse_response',
    'c_rns': 'content_render_start',
    'c_rnf': 'content_render_finish',
    'remove_splash': 'remove_splash',
    'ftl': 'first_thumbnail_load',
    'cpt': 'content_paint_time',
    'pljs_rq': 'player_js_request',
    'pljs_r': 'player_js_ready',
    'pem_rq': 'player_embed_request',
    'fs': 'player_start',
    'pem_r': 'player_embed_ready',
    'gv': 'get_video',
    # Not from CSI
    'cobalt_appStart': 'cobalt_appstart',
    'cobalt_startTime': 'cobalt_startTime',
})


def decode(tick):
  if tick in tick_names:
    return tick_names[tick] + '(' + tick + ')'
  return tick


COBALT_WEBDRIVER_CAPABILITIES = {
    'browserName': 'cobalt',
    'javascriptEnabled': True,
    'platform': 'LINUX',
}


def adb(*cmd):
  cmd = ['adb'] + list(cmd)
  logging.debug('%s', ' '.join(cmd))
  subprocess.check_call(cmd, close_fds=True)


def test_android(app: str, wipe: bool, extra_delay_seconds: int):
  adb('wait-for-device')
  adb('shell', 'am', 'force-stop', app)
  if wipe:
    logging.info('Wiping storage')
    adb('shell', 'pm', 'clear', app)
  adb('forward', 'tcp:4444', 'tcp:4444')
  adb('logcat', '-c')
  time.sleep(0.2)  # for stop to work
  adb(
      'shell',
      'am',
      'start',
      '--esa',
      'args',
      '"--dev_servers_listen_ip=127.0.0.1"',
      app,
  )
  executor = remote_connection.RemoteConnection(android_webdriver)
  executor.set_timeout(2)
  wd = None
  connection_timeout = 500
  while connection_timeout:
    with contextlib.suppress(urllib3.exceptions.ProtocolError):
      wd = webdriver.Remote(executor, COBALT_WEBDRIVER_CAPABILITIES)
    if wd:
      break
    time.sleep(0.1)
    connection_timeout -= 1
  log.info('url: %s', wd.current_url)
  time.sleep(3)
  origin = wd.execute_script('return performance.timeOrigin;')
  time.sleep(2 + extra_delay_seconds)
  csi = wd.execute_script('return window.ytcsi.debug[0];')
  has_ur = wd.execute_script('return window.ytcsi.debug.hasUR;')
  now_ts = wd.execute_script('return performance.now();')
  lifecycle_entries = wd.execute_script(
      'return performance.getEntriesByType("lifecycle")[0];')
  navigation_entries = wd.execute_script(
      'return performance.getEntriesByType("navigation")[0];')
  wd.quit()
  ticks = csi['tick']
  # Those are milliseconds, can show just ints
  rebased_ticks = {key: int(value - origin) for key, value in ticks.items()}
  sorted_ticks = collections.OrderedDict(
      sorted(rebased_ticks.items(), key=lambda item: item[1]))
  returnticks = {}
  for key, value in sorted_ticks.items():
    if key in tick_names:
      log.debug(decode(key), value)
      returnticks[key] = value
  log.debug(now_ts)
  returnticks['cobalt_appStart'] = lifecycle_entries['appStart']
  returnticks['cobalt_startTime'] = navigation_entries['startTime']
  log.info('Has unexpected restart: %s', has_ur)
  return returnticks


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Startup test')
  parser.add_argument(
      '-l', '--loops', type=int, default=10, help='Number of app starts to run')
  parser.add_argument(
      '-v', '--verbose', action='store_true', help='Verbose output')
  parser.add_argument(
      '-e',
      '--extra',
      type=int,
      default=0,
      help='Extra wait delay in seconds, for slow devices',
  )
  parser.add_argument(
      '-o',
      '--output',
      type=str,
      default='file.csv',
      help='Output CSV filename')
  parser.add_argument(
      '-w',
      '--wipe',
      choices=['never', 'first', 'always'],
      default='never',
      help='When to wipe local storage',
  )
  parser.add_argument(
      '-a',
      '--app',
      choices=['coat', 'youtube', 'youtubetv'],
      default='coat',
      help='Which Android TV app to use',
  )
  args = parser.parse_args()
  logging.basicConfig(
      level=logging.DEBUG if args.verbose else logging.INFO,
      handlers=[logging.StreamHandler()],
  )

  app_id = {
      'coat': 'dev.cobalt.coat',
      'youtube': 'com.google.android.youtube.tv',
      'youtubetv': 'com.google.android.youtube.tvunplugged',
  }[args.app]
  all_runs = []
  with open(args.output, 'w', encoding='utf-8') as file:
    writer = csv.writer(file)
    headings = [decode(x) for x in tick_names.keys()]
    writer.writerow(headings)
    for f in range(0, args.loops):
      do_wipe = (args.wipe == 'first' and f == 0) or (args.wipe == 'always')
      test_ticks = test_android(app_id, do_wipe, args.extra)
      writer.writerow(test_ticks.values())
      file.flush()
      all_runs.append(test_ticks)
  log.debug('%s', all_runs)
  log.info(
      'cobalt_appstart %s',
      statistics.median([x['cobalt_appStart'] for x in all_runs]),
  )
  log.info(
      'cobalt_startTime %s',
      statistics.median([x['cobalt_startTime'] for x in all_runs]),
  )

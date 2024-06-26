from cobalt.tools.automated_testing import webdriver_utils
import time
import starboard.android.shared.launcher
import contextlib
import urllib3.exceptions
from collections import OrderedDict
import csv
import argparse

# pylint: disable=protected-access

# android_devtools =  'http://127.0.0.1:9222'
android_webdriver = 'http://127.0.0.1:4444'

# youtube/web/core/logging/csi/csi_tick.ts
# +
# video/youtube/web/living_room/core/logging/csi.ts
tick_names = OrderedDict({
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
})


def decode(tick):
  if tick in tick_names:
    return tick_names[tick] + '(' + tick + ')'
  return tick


COBALT_WEBDRIVER_CAPABILITIES = {
    'browserName': 'cobalt',
    'javascriptEnabled': True,
    'platform': 'LINUX'
}


def test_android():
  launch = starboard.android.shared.launcher.Launcher(
      1, 2, 3, None, launcher_args=['systools', 'noinstall'])
  launch._CheckCallAdb('wait-for-device')
  launch._CheckCallAdb('shell', 'am', 'force-stop', 'dev.cobalt.coat')
  launch._CheckCallAdb('forward', 'tcp:4444', 'tcp:4444')
  launch._CheckCallAdb('logcat', '-c')
  time.sleep(0.2)  # for stop to work
  launch._CheckCallAdb('shell', 'am', 'start', '--esa', 'args',
                       '"--dev_servers_listen_ip=127.0.0.1"', 'dev.cobalt.coat')
  selenium_webdriver_module = webdriver_utils.import_selenium_module(
      'webdriver')
  rc = selenium_webdriver_module.remote.remote_connection
  executor = rc.RemoteConnection(android_webdriver)
  executor.set_timeout(2)
  webdriver = None
  while True:
    with contextlib.suppress(urllib3.exceptions.ProtocolError):
      webdriver = selenium_webdriver_module.Remote(
          executor, COBALT_WEBDRIVER_CAPABILITIES)
    if webdriver:
      break
    time.sleep(0.1)
  print('url:' + webdriver.current_url)
  time.sleep(3)
  origin = webdriver.execute_script('return performance.timeOrigin;')
  time.sleep(2)
  csi = webdriver.execute_script('return window.ytcsi.debug[0];')
  now_ts = webdriver.execute_script('return performance.now();')
  webdriver.quit()
  ticks = csi['tick']
  # Those are milliseconds, can show just ints
  rebased_ticks = {key: int(value - origin) for key, value in ticks.items()}
  sorted_ticks = OrderedDict(
      sorted(rebased_ticks.items(), key=lambda item: item[1]))
  returnticks = {}
  for key, value in sorted_ticks.items():
    if key in tick_names:
      print(decode(key), value)
      returnticks[key] = value
  print(now_ts)
  return returnticks


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Startup test')
  parser.add_argument(
      '-l', '--loops', type=int, default=10, help='Number of starts')
  parser.add_argument(
      '-o',
      '--output',
      type=str,
      default='file.csv',
      help='Output CSV filename')
  args = parser.parse_args()

  with open(args.output, 'w', encoding='utf-8') as file:
    writer = csv.writer(file)
    headings = [decode(x) for x in tick_names.keys()]
    writer.writerow(headings)
    for f in range(1, args.loops):
      test_ticks = test_android()
      writer.writerow(test_ticks.values())
      file.flush()

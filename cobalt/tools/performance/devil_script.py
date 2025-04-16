# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" A script to run YTLR performance tests.
"""

import argparse
import logging
import re
import requests
import sys
import time

import devil_chromium
from devil.android import device_errors
from devil.android import device_utils
from devil.android.sdk import intent
from devil.android.tools import script_common
from devil.utils import logging_common
from telemetry.timeline import chrome_trace_config

from telemetry.internal.backends.chrome_inspector import devtools_client_backend


class Forwarder(object):
  _local_port = 9222
  _remote_port = 9223

  def Close(self):
    pass


class FakeFactory(object):

  def Create(self, *args, **kwargs):
    print(args, kwargs)
    logging.info('Create called')
    return Forwarder()


class FakeTraceConfig:

  def __init__(self):
    self._chrome_trace_config = chrome_trace_config.ChromeTraceConfig()

  @property
  def chrome_trace_config(self):
    return self._chrome_trace_config


class FakeTracingControllerBackend:

  def __init__(self):
    self._chrome_trace_config = FakeTraceConfig()

  def GetChromeTraceConfig(self):
    return self._chrome_trace_config


class FakeBackend(object):

  def __init__(self):
    self.forwarder_factory = FakeFactory()
    self.tracing_controller_backend = FakeTracingControllerBackend()

  def GetOSName(self):
    # Lie about our OS, so browser_target_url() doesn't need modifications
    return 'castos'


class FakeConn(object):
  platform_backend = FakeBackend()


def MonkeyPatchVersionNumber(self):
  print('Monkey-patching GetChromeMajorNumber')
  resp = self.GetVersion()
  if 'Protocol-Version' in resp:
    major_number_match = re.search(r'Cobalt/(\d+)', resp['User-Agent'])
    if major_number_match:
      # Simply return something believable
      return 114
  return 0


class PerfTesting():
  """
  A class that generally supports perf testing for Cobalt.
  """

  def __init__(self):
    devtools_client_backend._DevToolsClientBackend.GetChromeMajorNumber =\
      MonkeyPatchVersionNumber
    self._InitFromCommandLineArgs()

  def GetMeminfoTotal(self, meminfo_output: str, value_type: str) -> int:
    """
      Extracts 'TOTAL PSS', 'TOTAL RSS', or 'TOTAL SWAP PSS' from meminfo.

      Args:
          meminfo_output: The string containing the dumpsys meminfo output.
          value_type: The type of total value to extract.
                      Case-insensitive. Accepts 'PSS', 'RSS', or 'SWAP PSS'.

      Returns:
          The extracted integer value in KB, or None if not found.
      """
    value_type = value_type.upper()
    if value_type == 'SWAP PSS':
      key_label = 'TOTAL SWAP PSS'
    elif value_type in ['PSS', 'RSS']:
      key_label = f'TOTAL {value_type}'
    else:
      print(f"Error: Invalid value_type specified: {value_type}. Use 'PSS', \
            'RSS', or 'SWAP PSS'.")
      return None

    # Regex to find the specific line containing all totals first
    # This makes it more robust against similar labels appearing elsewhere
    total_line_match = re.search(
        r'TOTAL PSS:\s+\d+\s+TOTAL RSS:\s+\d+\s+TOTAL SWAP PSS:\s+\d+',
        meminfo_output)

    if not total_line_match:
      return None

    # Get the full line like 'TOTAL PSS: 123 TOTAL RSS: 456 ...'
    total_line = total_line_match.group(0)

    # Now search for the specific key and value within that line
    # Pattern: The specific label (e.g., 'TOTAL RSS'), colon,
    #          whitespace, capture digits
    pattern = rf'{re.escape(key_label)}:\s+(\d+)'
    match = re.search(pattern, total_line)

    if match:
      value_str = match.group(1)
      try:
        return int(value_str)
      except ValueError:
        # This shouldn't happen with \d+ but good practice
        print(f'Warning: Could not convert found value \
          "{value_str}" to integer.')
        return None
    else:
      # This case should be rare if total_line_match succeeded,
      # but handles potential regex mismatches.
      return None

  def _InitFromCommandLineArgs(self):
    parser = argparse.ArgumentParser(description='Test')
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--is_cobalt', nargs='*', default='False')

    logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)

    script_common.AddEnvironmentArguments(parser)
    script_common.AddDeviceArguments(parser)
    logging_common.AddLoggingArguments(parser)

    args = parser.parse_args()
    args_dict = vars(args)
    cobalt_key = None
    if '-c' in args_dict.keys():
      cobalt_key = 'c'
    elif 'is_cobalt' in args_dict.keys():
      cobalt_key = 'is_cobalt'

    if cobalt_key is not None:
      cobalt_arg = args_dict[cobalt_key][0]
      if cobalt_arg in ('True', 'true', 'T', 't'):
        self.is_cobalt = True
      elif cobalt_arg in ('False', 'false', 'F', 'f'):
        self.is_cobalt = False
      else:
        print('Usage: devil_script.py [-c/--is_cobalt T/t/True/False/F/f]')
        raise ValueError(
            'Expected a boolean value for -c/--is_cobalt argument but was ' +
            cobalt_arg)

      logging_common.InitializeLogging(args)
      devil_chromium.Initialize(adb_path=args.adb_path)
      self.logcat_timeout = 10

  def RunTests(self):
    devs = list(device_utils.DeviceUtils.HealthyDevices())
    dev = devs[0]
    print(f'{dev}')
    dev.adb.Forward(
        'tcp:9222',
        'localabstract:content_shell_devtools_remote',
        allow_rebind=True)

    res = dev.adb.Shell('free -h')
    lines = res.splitlines()
    splits = re.split(' +', lines[1])
    used = splits[2]
    free = splits[3]
    print(f'used {used} free {free}')

    logcat_monitor = dev.GetLogcatMonitor(clear=True)
    logcat_monitor.Start()
    result_line_re = re.compile(
        r'starboard: Successfully retrieved Advertising ID')

    xtras = {
        'commandLineArgs':
            '--remote-allow-origins=*,\
        --url="https://www.youtube.com/tv?automationRoutine=browseWatchRoutine"'
    }
    if not self.is_cobalt:
      pkg = 'com.google.android.youtube.tv'
      act = 'com.google.android.apps.youtube.tv.activity.ShellActivity'
    else:
      pkg = 'dev.cobalt.coat'
      act = 'dev.cobalt.app.MainActivity'

    launch_intent = intent.Intent(
        package=pkg,
        activity=act,
        action='android.intent.action.MAIN',
        category='[android.intent.category.LAUNCHER]',
        extras=xtras)
    dev.StartActivity(
        launch_intent,
        force_stop=True,
    )
    print(launch_intent.am_args)
    print('\n')

    try:
      logcat_monitor.WaitFor(result_line_re, timeout=self.logcat_timeout)
    except device_errors.CommandTimeoutError as _:
      logging.warning('Timeout waiting for the result line')
    logcat_monitor.Stop()
    logcat_monitor.Close()
    time.sleep(2)

    # am start --esa commandLineArgs '--remote-allow-origins=*'
    # gets mangle-quoted for some reason
    response_object = requests.get('http://localhost:9222/json').json()
    print(response_object)
    print('\n')
    page = [x for x in response_object if x['type'] == 'page'][0]
    print(page)
    print('\n')
    tab_id = page['id']
    print(f'go/cobaltrdt/{tab_id }')
    print('\n')
    ws = page['webSocketDebuggerUrl']
    print(ws)
    print('\n')
    fakeconn = FakeConn()
    backend = devtools_client_backend.GetDevToolsBackEndIfReady(
        devtools_port=9222, app_backend=fakeconn, enable_tracing=False)
    print(backend.GetVersion())
    print(backend.GetUrl(tab_id))
    sysinfo = backend.GetSystemInfo()
    print(sysinfo.model_name, sysinfo.gpu.devices[0])

    out = dev.adb.Shell(f'dumpsys meminfo {pkg}')
    print('Total RSS: ', self.GetMeminfoTotal(out, 'RSS'))
    print('Total PSS: ', self.GetMeminfoTotal(out, 'PSS'))
    print('SWAP PSS: ', self.GetMeminfoTotal(out, 'SWAP PSS'))

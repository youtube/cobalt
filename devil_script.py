
import os
import sys
import argparse

import logging
import re
import sys
import requests
import time

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), 'build', 'android'))
print(os.path.abspath(__file__))
REPOSITORY_ROOT = os.path.abspath('.')

sys.path.append(os.path.join(REPOSITORY_ROOT, 'third_party','catapult', 'devil'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'third_party',
                             'catapult', 'telemetry'))

import devil_chromium

from devil.android import device_errors
from devil.android import device_utils
from devil.android.sdk import intent
from devil.android.tools import script_common
from devil.android.tools import system_app
from devil.utils import logging_common

def MonkeyPatchVersionNumber(self):
  print(f'Monkey-patching GetChromeMajorNumber')
  resp = self.GetVersion()
  if 'Protocol-Version' in resp:
    major_number_match = re.search(r'Cobalt/(\d+)',
                                    resp['User-Agent'])
    if major_number_match:
      # Simply return something believable
      return 114
  return 0

from telemetry.internal.backends.chrome_inspector import devtools_client_backend
from telemetry.internal.backends.chrome import android_browser_backend

devtools_client_backend._DevToolsClientBackend.GetChromeMajorNumber = MonkeyPatchVersionNumber

class Forwarder(object):
  _local_port = 9222
  _remote_port = 9223
  def Close(self):
    None

class FakeFactory(object):
  def Create(*args,**kwargs):
    logging.info("Create called")
    return Forwarder()

class FakeBackend(object):
  forwarder_factory = FakeFactory()
  def GetOSName(self):
    # Lie about our OS, so browser_target_url() doesn't need modifications
    return "castos"

class FakeConn(object):
  platform_backend = FakeBackend()

def main():
  logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
  parser = argparse.ArgumentParser(description="Test")

  script_common.AddEnvironmentArguments(parser)
  script_common.AddDeviceArguments(parser)
  logging_common.AddLoggingArguments(parser)

  args = parser.parse_args()
  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb_path)
  logcat_timeout = 10

  devs = list(device_utils.DeviceUtils.HealthyDevices())
  dev = devs[0]
  print(f'{dev}')
  dev.adb.Forward("tcp:9222","localabstract:content_shell_devtools_remote",allow_rebind=True)
  
  logcat_monitor = dev.GetLogcatMonitor(clear=True)
  logcat_monitor.Start()
  result_line_re = re.compile(r'starboard: Successfully retrieved Advertising ID')
  launch_intent = intent.Intent(
      package="com.google.android.youtube.tv",
      activity="com.google.android.apps.youtube.tv.activity.ShellActivity",
      action="android.intent.action.MAIN",
      extras={
      'commandLineArgs': "--remote-allow-origins=*"
    }
  )
  dev.StartActivity(
    launch_intent,
    force_stop=True,
  )

  try:
    match = logcat_monitor.WaitFor(result_line_re, timeout=logcat_timeout)
  except device_errors.CommandTimeoutError as _:
    logging.warning('Timeout waiting for the result line')
  logcat_monitor.Stop()
  logcat_monitor.Close()
  time.sleep(2)

  # am start --esa commandLineArgs '--remote-allow-origins=*' < this gets mangle-quoted for some reason
  response_object = requests.get("http://localhost:9222/json").json()
  print(response_object)
  page = [x for x in response_object if x['type']=='page'][0]
  print(page)
  tab_id = page['id']
  print(f'go/cobaltrdt/{tab_id }')
  ws = page['webSocketDebuggerUrl']
  print(ws)
  fakeconn = FakeConn()
  backend = devtools_client_backend.GetDevToolsBackEndIfReady(
    devtools_port=9222,app_backend=fakeconn, enable_tracing=False)
  print(backend.GetVersion())
  print(backend.GetUrl(tab_id))
  sysinfo = backend.GetSystemInfo()
  print(sysinfo.model_name, sysinfo.gpu.devices[0])
  tab_backend = backend.FirstTabBackend()
  result = tab_backend.EvaluateJavaScript('41+1')
  print(result)


if __name__ == '__main__':
  main()

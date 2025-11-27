import os
import sys
import argparse
import logging
import re
import time
import requests

# Setup paths and apply monkeypatches
import setup_paths
import monkeypatches

import devil_chromium
from devil.android import device_errors
from devil.android import device_utils
from devil.android.sdk import intent
from devil.android.tools import script_common
from devil.utils import logging_common

# Import the fake connection object we still need
from cobalt.tools.catapult.monkeypatch.fake_android_backend import FakeConn
from telemetry.internal.backends.chrome_inspector import devtools_client_backend


def get_meminfo_total(meminfo_output: str, value_type: str) -> int:
  """
    Extracts 'TOTAL PSS', 'TOTAL RSS', or 'TOTAL SWAP PSS' from meminfo output.

    Args:
        meminfo_output: The string containing the dumpsys meminfo output.
        value_type: The type of total value to extract.
                    Case-insensitive. Accepts 'PSS', 'RSS', or 'SWAP PSS'.

    Returns:
        The extracted integer value in KB, or None if not found.
    """
  value_type = value_type.upper()
  if value_type == "SWAP PSS":
    key_label = "TOTAL SWAP PSS"
  elif value_type in ["PSS", "RSS"]:
    key_label = f"TOTAL {value_type}"
  else:
    logging.error(
        f"Error: Invalid value_type specified: {value_type}. Use 'PSS', 'RSS', or 'SWAP PSS'."
    )
    return None

  # Regex to find the specific line containing all totals first
  # This makes it more robust against similar labels appearing elsewhere
  total_line_match = re.search(
      r"TOTAL PSS:\s+\d+\s+TOTAL RSS:\s+\d+\s+TOTAL SWAP PSS:\s+\d+",
      meminfo_output)

  if not total_line_match:
    # print(f"Info: Line containing '{key_label}' not found.")
    return None

  total_line = total_line_match.group(
      0)  # Get the full line like "TOTAL PSS: 123 TOTAL RSS: 456 ..."

  # Now search for the specific key and value within that line
  # Pattern: The specific label (e.g., "TOTAL RSS"), colon, whitespace, capture digits
  pattern = rf"{re.escape(key_label)}:\s+(\d+)"
  match = re.search(pattern, total_line)

  if match:
    value_str = match.group(1)
    try:
      return int(value_str)
    except ValueError:
      # This shouldn't happen with \d+ but good practice
      logging.warning(
          f"Warning: Could not convert found value '{value_str}' to integer.")
      return None
  else:
    # This case should be rare if total_line_match succeeded,
    # but handles potential regex mismatches.
    # print(f"Info: Key '{key_label}' not found within the identified total line.")
    return None


def main():
  logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
  log = logging.getLogger(__name__)
  parser = argparse.ArgumentParser(description="Test")

  script_common.AddEnvironmentArguments(parser)
  script_common.AddDeviceArguments(parser)
  logging_common.AddLoggingArguments(parser)
  parser.add_argument(
      '-a',
      '--app',
      choices=['coat', 'youtube', 'youtubetv'],
      default='coat',
      help='Which Android TV app to use',
  )
  parser.add_argument(
      '--devtools-port',
      type=int,
      default=9222,
      help='Local port for remote debugging.',
  )

  args = parser.parse_args()
  logging_common.InitializeLogging(args)
  devil_chromium.Initialize(adb_path=args.adb_path)
  logcat_timeout = 10

  devs = list(device_utils.DeviceUtils.HealthyDevices())
  dev = devs[0]
  log.info(f'{dev}')
  dev.adb.Forward(
      f"tcp:{args.devtools_port}",
      "localabstract:content_shell_devtools_remote",
      allow_rebind=True)

  res = dev.adb.Shell("free -h")
  lines = res.splitlines()
  splits = re.split(' +', lines[1])
  used = splits[2]
  free = splits[3]
  log.info(f"used {used} free {free}")

  app_id = {
      'coat': 'dev.cobalt.coat',
      'youtube': 'com.google.android.youtube.tv',
      'youtubetv': 'com.google.android.youtube.tvunplugged',
  }[args.app]
  PACKAGE = app_id

  launch_intent = intent.Intent(
      package=PACKAGE,
      activity="com.google.android.apps.youtube.tv.activity.ShellActivity",
      action="android.intent.action.MAIN",
      extras={'commandLineArgs': "--remote-allow-origins=*"})
  dev.StartActivity(
      launch_intent,
      force_stop=True,
  )

  time.sleep(5)

  # am start --esa commandLineArgs '--remote-allow-origins=*' < this gets mangle-quoted for some reason
  response_object = requests.get(
      f"http://localhost:{args.devtools_port}/json").json()
  log.info(response_object)
  page = [x for x in response_object if x['type'] == 'page'][0]
  log.info(page)
  tab_id = page['id']
  log.info(f'go/cobaltrdt/{tab_id }')
  ws = page['webSocketDebuggerUrl']
  log.info(ws)
  fakeconn = FakeConn()
  backend = devtools_client_backend.GetDevToolsBackEndIfReady(
      devtools_port=args.devtools_port,
      app_backend=fakeconn,
      enable_tracing=False)
  log.info(backend.GetVersion())
  log.info(backend.GetUrl(tab_id))
  sysinfo = backend.GetSystemInfo()
  log.info(f'{sysinfo.model_name}, {sysinfo.gpu.devices[0]}')
  tab_backend = backend.FirstTabBackend()
  result = tab_backend.EvaluateJavaScript('41+1')
  log.info(result)

  out = dev.adb.Shell(f'dumpsys meminfo {PACKAGE}')
  log.info("Total RSS: %s", get_meminfo_total(out, "RSS"))

  # backend.ExecuteBrowserCommand('Browser.getHistograms')
  request = {'method': 'Browser.getHistograms', 'params': {'delta': False}}
  res = backend._browser_websocket.SyncRequest(request, 2)
  log.info("Response: {}", res)


if __name__ == '__main__':
  main()

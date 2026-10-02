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
"""Smoke test verifying Cobalt can be driven by Chromedriver."""

import argparse
import logging
import os
import sys
import time

try:
  from selenium import webdriver
  from selenium.webdriver.chrome.options import Options
  from selenium.webdriver.chrome.service import Service as ChromeService
except ImportError as e:
  raise RuntimeError('Please install `selenium`, pip install selenium'
                     ' OR apt-get install python3-selenium') from e

LOCAL_WEBDRIVER = 'http://127.0.0.1:4444'
HOMEDIR = os.getenv('HOME', '/tmp')

BASEDIR_LOCAL = f'{HOMEDIR}/code/chromium/src'
DRIVER_LOCAL = f'{BASEDIR_LOCAL}/out/linux-x64x11_qa/chromedriver'
BINARY_LOCAL = f'{BASEDIR_LOCAL}/out/linux-x64x11_qa/cobalt'
LOGS_SCREENSHOTS_DIR = f'{HOMEDIR}/code/chromedriver_tests'


def get_browser_args(output_dir):
  return [
      '--allow-browser-signin=false',
      '--allow-pre-commit-input',
      '--allow-running-insecure-content',
      '--allow_http',
      '--csp-enforcement=false',
      '--csp_mode=disable',
      '--deterministic-mode',
      '--disable-background-timer-throttling',
      '--disable-blink-features='
      'ShadowDOMV0,CustomElementsV0,HTMLImports,MutationEvents',
      '--disable-component-update',
      '--disable-features=PersistentOriginTrials,Vulkan',
      '--disable-font-subpixel-positioning',
      '--disable-fre',
      '--disable-gpu-rasterization',
      '--disable-partial-raster',
      '--disable-skia-runtime-opts',
      '--disable-splash-screen',
      '--disallow-signin',
      '--enable-features=DisableSplashScreen',
      '--enable-logging=stderr',
      '--force-device-scale-factor=1',
      '--force-video-overlays',
      '--https-enforcement=false',
      '--ignore-gpu-blocklist',
      '--kiosk',
      f'--log-file={output_dir}/logs/chrobalt.log',
      '--log-level=0',
      '--no-first-run',
      '--no-sandbox',
      '--ozone-platform=starboard',
      '--remote-allow-origins=*',
      '--remote-debugging-port=9222',
      '--single-process',
      '--single-process-tests',
      '--use-angle=gles-egl',
      '--use-gl=angle',
      '--user-level-memory-pressure-signal-params',
      '--window-position=0,0',
  ]


def connect_webdriver(chrome_binary_path, chrome_driver_path, output_dir):
  os.makedirs(f'{output_dir}/logs', exist_ok=True)
  os.makedirs(f'{output_dir}/screenshots', exist_ok=True)

  chrome_options = Options()
  chrome_options.binary_location = chrome_binary_path

  for arg in get_browser_args(output_dir):
    chrome_options.add_argument(arg)

  chrome_options.add_experimental_option('excludeSwitches',
                                         ['enable-automation'])

  service = ChromeService(
      executable_path=chrome_driver_path,
      port=4444,
      service_args=[
          '--verbose',
          f'--log-path={output_dir}/logs/chromedriver.log',
      ],
  )
  driver = webdriver.Chrome(service=service, options=chrome_options)

  try:
    driver.get('https://www.youtube.com/tv')
    sleep_time = 5
    logging.info('sleeping for %d secs ...', sleep_time)
    time.sleep(sleep_time)

    screenshot_dir = f'{output_dir}/screenshots'
    num_files = sum(1 for entry in os.listdir(screenshot_dir)
                    if os.path.isfile(os.path.join(screenshot_dir, entry)))
    img_name = f'{screenshot_dir}/{num_files}.png'
    logging.info('screenshotting ...')
    driver.save_screenshot(img_name)
    if os.path.exists(img_name) and os.path.getsize(img_name) > 0:
      logging.info('screenshot saved to %s', img_name)
    else:
      raise RuntimeError(f'Failed to save screenshot to {img_name}')
  finally:
    driver.quit()


def main():
  parser = argparse.ArgumentParser(description='Startup test')
  parser.add_argument(
      '-v', '--verbose', action='store_true', help='Verbose output')
  parser.add_argument('-b', '--binary', default=BINARY_LOCAL)
  parser.add_argument('-d', '--driver', default=DRIVER_LOCAL)
  parser.add_argument('-o', '--output-dir', default=LOGS_SCREENSHOTS_DIR)
  args = parser.parse_args()
  logging.basicConfig(
      level=logging.DEBUG if args.verbose else logging.INFO,
      handlers=[logging.StreamHandler()],
  )

  if not os.path.exists(args.binary):
    logging.error('Chrobalt/Chrome binary not found at: %s', args.binary)
    sys.exit(1)

  if not os.path.exists(args.driver):
    logging.error('Chromedriver not found at: %s', args.driver)
    sys.exit(1)

  logging.info('Driver to run: %s', args.driver)
  logging.info('Binary to run: %s', args.binary)
  connect_webdriver(args.binary, args.driver, args.output_dir)
  logging.info('ok')


if __name__ == '__main__':
  main()

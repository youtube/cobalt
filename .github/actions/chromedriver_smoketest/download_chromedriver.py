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
"""Helper utility to download official Chromedriver binary matching version."""

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
import zipfile


def download_chromedriver(version, dest_dir):
  try:
    parts = version.split('.')
    major = int(parts[0])
  except ValueError:
    print(f'Error parsing version: {version}')
    sys.exit(1)

  if major < 115:
    url = (f'https://chromedriver.storage.googleapis.com/{version}/'
           'chromedriver_linux64.zip')
  else:
    url = ('https://storage.googleapis.com/chrome-for-testing-public/'
           f'{version}/linux64/chromedriver-linux64.zip')

  zip_path = os.path.join(dest_dir, 'chromedriver.zip')
  print(f'Downloading from {url}...')
  try:
    urllib.request.urlretrieve(url, zip_path)
  except urllib.error.URLError as e:
    print(f'Failed to download: {e}')
    if major >= 115:
      print('Falling back to resolving latest patch version...')
      api_url = ('https://googlechromelabs.github.io/chrome-for-testing/'
                 'known-good-versions-with-downloads.json')
      with urllib.request.urlopen(api_url) as req:
        data = json.loads(req.read().decode('utf-8'))
      for v in data['versions']:
        if v['version'].startswith(f'{major}.'):
          if 'chromedriver' in v['downloads']:
            for d in v['downloads']['chromedriver']:
              if d['platform'] == 'linux64':
                url = d['url']
      print(f'Resolved to {url}')
      urllib.request.urlretrieve(url, zip_path)
    else:
      raise

  with zipfile.ZipFile(zip_path, 'r') as zip_ref:
    zip_ref.extractall(dest_dir)

  os.remove(zip_path)

  for root, _, files in os.walk(dest_dir):
    if 'chromedriver' in files:
      bin_path = os.path.join(root, 'chromedriver')
      os.chmod(bin_path, 0o755)
      print(f'Chromedriver downloaded and extracted to: {bin_path}')
      return bin_path
  return None


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'version', help='Chromedriver version to download (e.g. 115.0.5790.170)')
  parser.add_argument(
      '--dest',
      default='/tmp/chromedriver_download',
      help='Destination directory')
  args = parser.parse_args()

  os.makedirs(args.dest, exist_ok=True)
  download_chromedriver(args.version, args.dest)


if __name__ == '__main__':
  main()

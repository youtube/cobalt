#!/usr/bin/env python3
# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
<<<<<<< HEAD
import base64
import json
import os
import io
import re
import zipfile
import urllib.request
from os import path

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

=======
import sys
import urllib.request
from os import path

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

def get_artifact_url(run, name):
  return f'https://storage.googleapis.com/perfetto-ci-artifacts/{run}/ui-test-artifacts/{name}'


def main():
<<<<<<< HEAD
  os.chdir(ROOT_DIR)
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'run',
      metavar='RUN',
      help='CI run identifier, e.g. ' +
      '\'20240923144821--cls-3258215-15--ui-clang-x86_64-release\'')
  args = parser.parse_args()
  url = get_artifact_url(args.run, 'index.html')
  with urllib.request.urlopen(url) as resp:
    handle_report(resp.read().decode('utf-8'), args.run)


def sanitize(name):
  return re.sub('[ _]', '-', name)


def handle_report(report: str, run: str):
  m = re.findall(
      r'playwrightReportBase64 = "data:application/zip;base64,([^"]+)"', report)
  bin = base64.b64decode(m[0])
  z = zipfile.ZipFile(io.BytesIO(bin))
  report = json.loads(z.open('report.json').read().decode())
  pngs = {}
  for f in report['files']:
    test_file = f['fileName'].removeprefix('test/')
    for t in f['tests']:
      title = sanitize(t['title'])
      for r in t['results']:
        for a in r['attachments']:
          png_name = sanitize(a['name'])
          if not png_name.endswith('-actual.png'):
            continue
          path = 'test/data/ui-screenshots/%s/%s/%s' % (
              test_file, title, png_name.replace('-actual', ''))
          pngs[path] = a['path']

  for local_path, remote_path in pngs.items():
    url = get_artifact_url(run, remote_path)
    print(f'Downloading {local_path} from {url}')
    urllib.request.urlretrieve(url, local_path)

  print('Done. Now run:')
  print('./tools/test_data upload  (or status)')
=======
  parser = argparse.ArgumentParser()
  parser.add_argument('run', metavar='RUN', help='CI run identifier')
  args = parser.parse_args()

  with urllib.request.urlopen(get_artifact_url(args.run, 'report.txt')) as resp:
    handle_report(resp.read().decode('utf-8'), args.run)


def handle_report(report: str, run: str):
  for line in report.split('\n'):
    if len(line) == 0:
      continue

    parts = line.split(';')
    if len(parts) != 2:
      print('Erroneous report line!')
      sys.exit(1)

    screenshot_name = parts[0]
    url = get_artifact_url(run, screenshot_name)
    output_path = path.join('test', 'data', 'ui-screenshots', screenshot_name)
    print(f'Downloading {url}')
    urllib.request.urlretrieve(url, output_path)
  print('Done. Now run:')
  print('./tools/test_data upload')
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))


if __name__ == "__main__":
  main()

#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
#
"""Builds and Packages Cobalt.

This is a simple macro to create local Android release packages mimicking what
is done by nightly builders.

To Run:
(cd chromium/src && cobalt/build/android/package.py)

Followed By (in a CitC instance):
cd video/youtube/tv/cobalt/piper_scripts
blaze run :android_release_to_piper -- --branch=main
  --snapshot_from_local=${HOME}/chromium/src/out/packages
"""

import argparse
import subprocess

PLATFORMS = ['android-arm', 'android-arm64', 'android-x86']
CONFIGS = ['gold', 'qa']


def build(platforms_to_package):
  for platform in platforms_to_package:
    for config in CONFIGS:
      subprocess.call(['cobalt/build/gn.py', '-p', platform, '-c', config])
      subprocess.call(
          ['autoninja', '-C', f'out/{platform}_{config}', 'cobalt:gn_all'])


def package(platforms_to_package):
  subprocess.call(['rm', '-rf', 'out/packages'])
  for platform in platforms_to_package:
    for config in CONFIGS:
      subprocess.call([
          'cobalt/build/packager.py',
          f'--name={platform}_{config}',
          '--json_path=cobalt/build/android/package.json',
          f'--out_dir=out/{platform}_{config}',
          f'--package_dir=out/packages/{platform}_local/local/local',
      ])


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--package_only',
      action='store_true',
      help='Skips build steps assuming that it is already complete')
  parser.add_argument(
      '--platforms',
      nargs='+',  # Expect one or more platform names
      help='Override the default platforms to build and package.')
  args = parser.parse_args()

  platforms = PLATFORMS
  if args.platforms:
    platforms = args.platforms

  if not args.package_only:
    build(platforms)

  package(platforms)

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
import os

BRANCH = 'main'
PLATFORMS = ['android-arm', 'android-arm64', 'android-x86']
CONFIGS = ['gold', 'qa']


def patch_args_for_google3(out_dir):
  """
  Ensures 'is_cobalt_on_google3 = true' is in the args.gn file.
  Will raise FileNotFoundError if args.gn is missing.
  """
  args_gn_path = os.path.join(out_dir, 'args.gn')

  with open(args_gn_path, 'r+', encoding='utf-8') as f:
    if not any(line.strip().startswith('is_cobalt_on_google3') for line in f):
      # The argument was not found, append it, ensuring a newline.
      f.write('\nis_cobalt_on_google3 = true\n')


def out_dir_for_google3(platform, config):
  return f'out/{platform}_{config}_for_google3'


def build(package_platforms):
  for platform in package_platforms:
    for config in CONFIGS:
      out_dir = out_dir_for_google3(platform, config)

      subprocess.call(
          ['cobalt/build/gn.py', out_dir, '-p', platform, '-c', config])

      patch_args_for_google3(out_dir)

      subprocess.call(['autoninja', '-C', out_dir, 'cobalt:gn_all'])


def package(package_platforms, package_branch):
  subprocess.call(['rm', '-rf', 'out/packages'])
  for platform in package_platforms:
    for config in CONFIGS:
      out_dir = out_dir_for_google3(platform, config)
      subprocess.call([
          'cobalt/build/packager.py',
          f'--name={platform}_{config}',
          '--json_path=cobalt/build/android/package.json',
          f'--out_dir={out_dir}',
          f'--package_dir=out/packages/{platform}_{package_branch}/local/local',
      ])


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--package_only',
      action='store_true',
      help='Skips build steps assuming that it is already complete.')
  parser.add_argument(
      '--platforms',
      nargs='+',  # Expect one or more platform names
      help='Override the default platforms to build and package.')
  parser.add_argument('--branch', help='The branch to use, e.g. 26.lts.1+')
  args = parser.parse_args()

  platforms = PLATFORMS
  if args.platforms:
    platforms = args.platforms
  branch = BRANCH
  if args.branch:
    branch = args.branch

  if not args.package_only:
    build(platforms)

  package(platforms, branch)

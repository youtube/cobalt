# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Darwin shared."""
# pylint: disable=R1732, C0301

import os
import shutil
import subprocess


def get_build_env(key):
  # Using readlines()
  gnfile = open('args.gn', 'r', encoding='utf-8')
  lines = gnfile.readlines()
  gnfile.close()
  for line in lines:
    if line.find(key) >= 0:
      return line[line.find('=') + 1:].strip()


framework_destination_path = './gen/intents_framework/YtIntent.framework'

print(subprocess.call('xcodebuild -version', shell=True))

target_platform_value = get_build_env('target_platform')
if target_platform_value.find('simulator') >= 0:

  subprocess.call(
      'xcodebuild -project '\
      '../../starboard/shared/uikit/intent/YtIntent.xcodeproj' \
      ' build  -scheme YtIntent -showdestinations',
      shell=True)
  # build x64 version for simulator
  subprocess.call(
      'xcodebuild -project '\
      '../../starboard/shared/uikit/intent/YtIntent.xcodeproj' \
      ' build  -scheme YtIntent -destination generic/platform="tvOS Simulator" ' \
      ' -configuration release' \
      ' -derivedDataPath ./gen/intents_framework',
      shell=True)

  if os.path.exists(framework_destination_path):
    shutil.rmtree(framework_destination_path)
  shutil.copytree(
      'gen/intents_framework/Build/Products/Release-appletvsimulator/YtIntent.framework',
      framework_destination_path)
else:
  # build arm64 version
  subprocess.call(
      'xcodebuild -project '\
      '../../starboard/shared/uikit/intent/YtIntent.xcodeproj' \
      ' build  -scheme YtIntent -destination generic/platform=tvOS ' \
      ' -configuration release' \
      ' -derivedDataPath ./gen/intents_framework',
      shell=True)
  if os.path.exists(framework_destination_path):
    shutil.rmtree(framework_destination_path)
  shutil.copytree(
      'gen/intents_framework/Build/Products/Release-appletvos/YtIntent.framework',
      framework_destination_path)

# Lint as: python2

# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
"""Utilities to use the toolchain from the Android NDK."""

import errno
import fcntl
import logging
import os
import re
import shutil
import subprocess
import sys
import time
import zipfile
import requests

from starboard.tools import build

# Which version of the Android NDK and CMake to install and build with.
# Note that build.gradle parses these out of this file too.
_NDK_VERSION = '21.1.6352462'
_CMAKE_VERSION = '3.10.2.4988404'

# Packages to install in the Android SDK.
# Get available packages from "sdkmanager --list --verbose"
_ANDROID_SDK_PACKAGES = [
    'build-tools;30.0.0',
    'cmake;' + _CMAKE_VERSION,
    'cmdline-tools;1.0',
    'emulator',
    'extras;android;m2repository',
    'extras;google;m2repository',
    'ndk;' + _NDK_VERSION,
    'patcher;v4',
    'platforms;android-30',
    'platform-tools',
]

# Seconds to sleep before writing "y" for android sdk update license prompt.
_SDK_LICENSE_PROMPT_SLEEP_SECONDS = 5

# Location from which to download the SDK command-line tools
# see https://developer.android.com/studio/index.html#command-tools
_SDK_URL = 'https://dl.google.com/android/repository/commandlinetools-linux-6200805_latest.zip'

# Location from which to download the Android NDK.
# see https://developer.android.com/ndk/downloads (perhaps in "NDK archives")
_NDK_ZIP_REVISION = 'android-ndk-r20b'
_NDK_ZIP_FILE = _NDK_ZIP_REVISION + '-linux-x86_64.zip'
_NDK_URL = 'https://dl.google.com/android/repository/' + _NDK_ZIP_FILE

_STARBOARD_TOOLCHAINS_DIR = build.GetToolchainsDir()

# The path to the Android SDK, if placed inside of starboard-toolchains.
_STARBOARD_TOOLCHAINS_SDK_DIR = os.path.join(_STARBOARD_TOOLCHAINS_DIR,
                                             'AndroidSdk')

_ANDROID_HOME = os.environ.get('ANDROID_HOME')
if _ANDROID_HOME:
  _SDK_PATH = _ANDROID_HOME
else:
  _SDK_PATH = _STARBOARD_TOOLCHAINS_SDK_DIR

_NDK_PATH = os.path.join(_SDK_PATH, 'ndk', _NDK_VERSION)

_SDKMANAGER_TOOL = os.path.join(_SDK_PATH, 'cmdline-tools', '1.0', 'bin',
                                'sdkmanager')


def GetNdkPath():
  return _NDK_PATH


def GetSdkPath():
  return _SDK_PATH


def _DownloadAndUnzipFile(url, destination_path):
  """Download a zip file from a url and uncompress it."""
  shutil.rmtree(destination_path, ignore_errors=True)
  try:
    os.makedirs(destination_path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise
  dl_file = os.path.join(destination_path, 'tmp.zip')
  request = requests.get(url, allow_redirects=True)
  open(dl_file, 'wb').write(request.content)
  _UnzipFile(dl_file, destination_path)


def _UnzipFile(zip_path, dest_path):
  """Extract all files and restore permissions from a zip file."""
  zip_file = zipfile.ZipFile(zip_path)
  for info in zip_file.infolist():
    zip_file.extract(info.filename, dest_path)
    os.chmod(os.path.join(dest_path, info.filename), info.external_attr >> 16L)


def InstallSdkIfNeeded():
  """Download the SDK and NDK if not already available."""
  # Hold an exclusive advisory lock on the _STARBOARD_TOOLCHAINS_DIR, to
  # prevent issues with modification for multiple variants.
  try:
    toolchains_dir_fd = os.open(_STARBOARD_TOOLCHAINS_DIR, os.O_RDONLY)
    fcntl.flock(toolchains_dir_fd, fcntl.LOCK_EX)

    if os.environ.get('ANDROID_NDK_HOME'):
      logging.warning('Warning: ANDROID_NDK_HOME is deprecated and ignored.')

    if _ANDROID_HOME:
      if not os.access(_SDKMANAGER_TOOL, os.X_OK):
        logging.error('Error: ANDROID_HOME is set but SDK is not present.')
        sys.exit(1)
      logging.warning('Warning: Using Android SDK in ANDROID_HOME,'
                      ' which is not automatically updated.\n'
                      '         The following package versions are installed:')
      installed_packages = _GetInstalledSdkPackages()
      for package in _ANDROID_SDK_PACKAGES:
        version = installed_packages.get(package, '< NOT INSTALLED >')
        msg = '  {:30} : {}'.format(package, version)
        logging.warning(msg)

      if not os.path.exists(GetNdkPath()):
        logging.error('Error: ANDROID_HOME is is missing NDK %s.', _NDK_VERSION)
        sys.exit(1)

      reply = raw_input(
          'Do you want to continue using your custom Android tools? [y/N]')
      if reply.upper() != 'Y':
        sys.exit(1)
    else:
      logging.warning('Checking Android SDK.')
      _DownloadInstallOrUpdateSdk()
  finally:
    fcntl.flock(toolchains_dir_fd, fcntl.LOCK_UN)
    os.close(toolchains_dir_fd)


def _GetInstalledSdkPackages():
  """Returns a map of installed package name to package version."""
  # We detect and parse either new-style or old-style output from sdkmanager:
  #
  # [new-style]
  # Installed packages:
  # --------------------------------------
  # build-tools;25.0.2
  #     Description:        Android SDK Build-Tools 25.0.2
  #     Version:            25.0.2
  #     Installed Location: <abs path>
  # ...
  #
  # [old-style]
  # Installed packages:
  #   Path               | Version | Description                    | Location
  #   -------            | ------- | -------                        | -------
  #   build-tools;25.0.2 | 25.0.2  | Android SDK Build-Tools 25.0.2 | <rel path>
  # ...
  section_re = re.compile(r'^[A-Z][^:]*:$')
  version_re = re.compile(r'^\s+Version:\s+(\S+)')

  p = subprocess.Popen([_SDKMANAGER_TOOL, '--list', '--verbose'],
                       stdout=subprocess.PIPE)

  installed_package_versions = {}
  new_style = False
  old_style = False
  for line in iter(p.stdout.readline, ''):
    # Throw away loading progress indicators up to the last CR.
    line = line.split('\r')[-1]
    if section_re.match(line):
      if new_style or old_style:
        # We left the new/old style installed packages section
        break
      if line.startswith('Installed'):
        # Read the header of this section to determine new/old style
        line = p.stdout.readline().strip()
        new_style = line.startswith('----')
        old_style = line.startswith('Path')
        if old_style:
          line = p.stdout.readline().strip()
        if not line.startswith('----'):
          logging.error('Error: Unexpected SDK package listing format')
      continue

    # We're in the installed packages section, and it's new-style
    if new_style:
      if not line.startswith(' '):
        pkg_name = line.strip()
      else:
        m = version_re.match(line)
        if m:
          installed_package_versions[pkg_name] = m.group(1)

    # We're in the installed packages section, and it's old-style
    elif old_style:
      fields = [f.strip() for f in line.split('|', 2)]
      if len(fields) >= 2:
        installed_package_versions[fields[0]] = fields[1]

  # Discard the rest of the output
  p.communicate()
  return installed_package_versions


def _IsUnattended():
  return 'BUILDBOT_BUILDERNAME' in os.environ or 'IS_DOCKER' in os.environ


def _DownloadInstallOrUpdateSdk():
  """Downloads (if necessary) and installs/updates the Android SDK."""

  # If we can't access the "sdkmanager" tool, we need to download the SDK
  if not os.access(_SDKMANAGER_TOOL, os.X_OK):
    logging.warning('Downloading Android SDK to %s',
                    _STARBOARD_TOOLCHAINS_SDK_DIR)
    if os.path.exists(_STARBOARD_TOOLCHAINS_SDK_DIR):
      shutil.rmtree(_STARBOARD_TOOLCHAINS_SDK_DIR)
    _DownloadAndUnzipFile(_SDK_URL, _STARBOARD_TOOLCHAINS_SDK_DIR)
    # TODO: Remove this workaround for sdkmanager incorrectly picking up the
    # "tools" directory from the ZIP as the name of its component.
    # https://issuetracker.google.com/issues/67495440#comment32
    # https://issuetracker.google.com/issues/150943631
    if not os.access(_SDKMANAGER_TOOL, os.X_OK):
      old_tools_dir = os.path.join(_STARBOARD_TOOLCHAINS_SDK_DIR, 'tools')
      new_tools_dir = os.path.join(_STARBOARD_TOOLCHAINS_SDK_DIR,
                                   'cmdline-tools', '1.0')
      os.mkdir(os.path.dirname(new_tools_dir))
      os.rename(old_tools_dir, new_tools_dir)
    if not os.access(_SDKMANAGER_TOOL, os.X_OK):
      logging.error('SDK download failed.')
      sys.exit(1)

  # Run the "sdkmanager" command with the appropriate packages

  if _IsUnattended():
    stdin = subprocess.PIPE
  else:
    stdin = sys.stdin

  p = subprocess.Popen(
      [_SDKMANAGER_TOOL, '--verbose'] + _ANDROID_SDK_PACKAGES, stdin=stdin)

  if _IsUnattended():
    try:
      # Accept "Terms and Conditions" (android-sdk-license)
      time.sleep(_SDK_LICENSE_PROMPT_SLEEP_SECONDS)
      p.stdin.write('y\n')
    except IOError:
      logging.warning('There were no SDK licenses to accept.')

  p.wait()
  if p.returncode:
    raise RuntimeError(
        '\nFailed to install packages with sdkmanager. Exit status = %d' %
        p.returncode)

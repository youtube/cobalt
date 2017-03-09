# Copyright 2016 Google Inc. All Rights Reserved.
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

import ConfigParser
import fcntl
import logging
import os
import platform
import shutil
import StringIO
import subprocess
import sys
import time
import urllib
import zipfile

import gyp_utils

# Packages to install in the Android SDK.
# Update these if you change the requirements below!
# Get available packages from "sdkmanager --list --verbose"
_ANDROID_SDK_PACKAGES = [
    'platforms;android-25',
    'build-tools;25.0.1',
    'extras;android;m2repository',
]
# NDK release to use
_NDK_ZIP_REVISION = 'android-ndk-r14'
# The "--api" to pass to the NDK's make_standalone_toolchain.py
_ANDROID_NDK_API_LEVEL = '21'
# The SDK version to compile Java against
_ANDROID_COMPILE_SDK_VERSION = '25'
# Version of Android build-tools to use
_ANDROID_BUILD_TOOLS_VERSION = '25.0.1'
# Min SDK for Java build
_ANDROID_MIN_SDK_VERSION = '21'
# Target SDK for Java build
_ANDROID_TARGET_SDK_VERSION = '22'

# Seconds to sleep before writing "y" for android sdk update license prompt.
_SDK_LICENSE_PROMPT_SLEEP_SECONDS = 5

_NDK_ZIP_FILE = ('%s-%s-x86_64.zip' %
                 (_NDK_ZIP_REVISION, platform.system().lower()))
_NDK_URL = 'https://dl.google.com/android/repository/%s' % _NDK_ZIP_FILE

_SDK_URL = 'https://dl.google.com/android/repository/tools_r25.2.3-linux.zip'

_ANDROID_HOME = os.environ.get('ANDROID_HOME')
_ANDROID_NDK_HOME = os.environ.get('ANDROID_NDK_HOME')

_SCRIPT_CTIME = os.path.getctime(__file__)
_COBALT_TOOLCHAINS_DIR = gyp_utils.GetToolchainsDir()

# The path to the Android SDK if placed inside of cobalt-toolchains
_COBALT_TOOLCHAINS_SDK_DIR = os.path.join(_COBALT_TOOLCHAINS_DIR, 'AndroidSdk')
# The path to the Android NDK if placed inside of cobalt-toolchains
_COBALT_TOOLCHAINS_NDK_DIR = os.path.join(_COBALT_TOOLCHAINS_SDK_DIR,
                                          'ndk-bundle')

# Maps the Android ABI to the architecture name of the toolchain.
_TOOLS_ABI_ARCH_MAP = {
    'x86'        : 'x86',
    'x86_64'     : 'x86_64',
    'armeabi'    : 'arm',
    'armeabi-v7a': 'arm',
    'arm64-v8a'  : 'arm64',
}

def GetToolsPath(abi):
  """Returns the path where the NDK standalone toolchain should be."""
  tools_arch = _TOOLS_ABI_ARCH_MAP[abi]
  tools_dir = 'android_toolchain_api%s_%s' % (_ANDROID_NDK_API_LEVEL, tools_arch)
  return os.path.realpath(os.path.join(_COBALT_TOOLCHAINS_DIR, tools_dir))


def GetEnvironmentVariables(abi):
  """Returns a dictionary of environment variables to provide to GYP."""
  tools_bin = os.path.join(GetToolsPath(abi), 'bin')
  return {
      'CC': os.path.join(tools_bin, 'clang'),
      'CXX': os.path.join(tools_bin, 'clang++'),
  }


def _CheckStamp(dir_path):
  """Checks that the specified directory is up-to-date with the NDK."""
  stamp_path = os.path.join(dir_path, 'ndk.stamp')
  return (
      os.path.exists(stamp_path) and
      os.path.getctime(stamp_path) > _SCRIPT_CTIME and
      _GetRevisionFromPropertiesFile(stamp_path) == _GetInstalledNdkRevision())


def UpdateStamp(dir_path):
  """Updates the stamp file in the specified directory to the NDK revision."""
  path = GetNdkPath()
  properties_path = os.path.join(path, 'source.properties')
  stamp_path = os.path.join(dir_path, 'ndk.stamp')
  shutil.copyfile(properties_path, stamp_path)


def GetNdkPath():
  # Prefer a developer-installed NDK if available
  # TODO: By default (which can be overridden explicitly) make sure all builds
  # happen with same NDK/SDK version.
  if _ANDROID_NDK_HOME:
    return _ANDROID_NDK_HOME
  elif _ANDROID_HOME:
    return os.path.join(_ANDROID_HOME, 'ndk-bundle')
  else:
    return _COBALT_TOOLCHAINS_NDK_DIR


def GetSdkPath():
  if _ANDROID_HOME:
    return _ANDROID_HOME
  else:
    return _COBALT_TOOLCHAINS_SDK_DIR


def _GetRevisionFromPropertiesFile(properties_path):
  with open(properties_path, 'r') as f:
    ini_str = '[properties]\n' + f.read()
  config = ConfigParser.RawConfigParser()
  config.readfp(StringIO.StringIO(ini_str))
  return config.get('properties', 'pkg.revision')


def _GetInstalledNdkRevision():
  """Returns the installed NDK's revision."""
  path = GetNdkPath()
  properties_path = os.path.join(path, 'source.properties')
  return _GetRevisionFromPropertiesFile(properties_path)


def InstallSdkIfNeeded(abi):
  """Installs appropriate SDK/NDK and NDK standalone tools if needed."""
  _MaybeDownloadAndInstallSdkAndNdk()
  _MaybeMakeToolchain(abi)


def GetJavacClasspath():
  """Returns classpath of jar files needed during Java compilation."""
  android_platform_dir = 'android-{}'.format(_ANDROID_COMPILE_SDK_VERSION)
  return ':'.join([
      os.path.join(GetSdkPath(), 'platforms', android_platform_dir,
                   'android.jar'),
      os.path.join(GetSdkPath(), 'extras', 'android', 'm2repository', 'com',
                   'android', 'support', 'support-annotations', '25.0.1',
                   'support-annotations-25.0.1.jar')
  ])


def GetBuildToolsPath():
  return os.path.join(GetSdkPath(), 'build-tools', _ANDROID_BUILD_TOOLS_VERSION)


def _IsSdkUpdateNeeded():
  """Returns true of 'android update sdk' must be run."""
  # Note that ideally there would be a better mechanism than simply
  # checking for required files. But there doesn't appear to be one...
  for f in GetJavacClasspath().split(':'):
    if not os.access(f, os.R_OK):
      return True
  if not os.path.exists(GetBuildToolsPath()):
    return True
  return False


def _DownloadAndUnzipFile(url, destination_path):
  dl_file, dummy_headers = urllib.urlretrieve(url)
  _UnzipFile(dl_file, destination_path)


def _UnzipFile(zip_path, dest_path):
  """Extract all files and restore permissions from a zip file."""
  zip_file = zipfile.ZipFile(zip_path)
  for info in zip_file.infolist():
    zip_file.extract(info.filename, dest_path)
    os.chmod(os.path.join(dest_path, info.filename), info.external_attr >> 16L)


def _MaybeDownloadAndInstallSdkAndNdk():
  """Download the SDK and NDK if not already available."""
  # Hold an exclusive advisory lock on the _COBALT_TOOLCHAINS_DIR, to
  # prevent issues with modification for multiple variants.
  try:
    toolchains_dir_fd = os.open(_COBALT_TOOLCHAINS_DIR, os.O_RDONLY)
    fcntl.flock(toolchains_dir_fd, fcntl.LOCK_EX)

    if _IsSdkUpdateNeeded():
      _DownloadInstallOrUpdateSdk()
    else:
      logging.warning('Android SDK up to date.')

    ndk_path = GetNdkPath()
    if not _ANDROID_HOME and not _ANDROID_NDK_HOME and not _CheckStamp(ndk_path):
      logging.warning(
          'NDK not found in either ANDROID_HOME nor ANDROID_NDK_HOME')
      logging.warning('Downloading NDK from %s to %s', _NDK_URL, ndk_path)
      if os.path.exists(ndk_path):
        shutil.rmtree(ndk_path)
      # Download directly to _COBALT_TOOLCHAINS_DIR since _NDK_ZIP_REVISION
      # directory is in the zip archive
      ndk_unzip_path = os.path.join(_COBALT_TOOLCHAINS_DIR, _NDK_ZIP_REVISION)
      if os.path.exists(ndk_unzip_path):
        shutil.rmtree(ndk_unzip_path)
      _DownloadAndUnzipFile(_NDK_URL, _COBALT_TOOLCHAINS_DIR)
      # Move NDK into its proper final place.
      os.rename(ndk_unzip_path, ndk_path)
      UpdateStamp(ndk_path)
  finally:
    fcntl.flock(toolchains_dir_fd, fcntl.LOCK_UN)
    os.close(toolchains_dir_fd)


def _IsOnBuildbot():
  return 'BUILDBOT_BUILDERNAME' in os.environ


def _DownloadInstallOrUpdateSdk():
  """Downloads (if necessary) and installs/updates the Android SDK."""
  android_sdk_tool_path = os.path.join(GetSdkPath(), 'tools', 'bin', 'sdkmanager')

  # If we can't access the "sdkmanager" tool, we need to download the SDK
  if not os.access(android_sdk_tool_path, os.X_OK):
    if _ANDROID_HOME:
      logging.error('ANDROID_HOME is set but SDK is not present!')
      sys.exit(1)
    logging.warning('Downloading Android SDK to %s', _COBALT_TOOLCHAINS_SDK_DIR)
    if os.path.exists(_COBALT_TOOLCHAINS_SDK_DIR):
      shutil.rmtree(_COBALT_TOOLCHAINS_SDK_DIR)
    _DownloadAndUnzipFile(_SDK_URL, _COBALT_TOOLCHAINS_SDK_DIR)
    if not os.access(android_sdk_tool_path, os.X_OK):
      logging.error('SDK download failed.')
      sys.exit(1)

  # Run the "sdkmanager" command with the appropriate packages

  if _IsOnBuildbot():
    stdin = subprocess.PIPE
  else:
    stdin = sys.stdin

  p = subprocess.Popen(
      [android_sdk_tool_path] + _ANDROID_SDK_PACKAGES,
      stdin=stdin)

  if _IsOnBuildbot():
    time.sleep(_SDK_LICENSE_PROMPT_SLEEP_SECONDS)
    p.stdin.write('y\n')

  p.wait()


def _MaybeMakeToolchain(abi):
  """Run the NDK's make_standalone_toolchain.py if necessary."""
  tools_arch = _TOOLS_ABI_ARCH_MAP[abi]
  tools_path = GetToolsPath(abi)
  if _CheckStamp(tools_path):
    logging.info('NDK %s toolchain already at %s', tools_arch,
                 _GetInstalledNdkRevision())
    return

  logging.warning('Installing NDK %s toolchain %s in %s', tools_arch,
                  _GetInstalledNdkRevision(), tools_path)

  if os.path.exists(tools_path):
    shutil.rmtree(tools_path)

  # Run the NDK script to make the standalone toolchain
  script_path = os.path.join(GetNdkPath(), 'build', 'tools',
                             'make_standalone_toolchain.py')
  args = [
      script_path, '--arch', tools_arch, '--api', _ANDROID_NDK_API_LEVEL,
      '--install-dir', tools_path
  ]
  script_proc = subprocess.Popen(args)
  rc = script_proc.wait()
  if rc != 0:
    raise RuntimeError('%s failed.' % script_path)

  UpdateStamp(tools_path)

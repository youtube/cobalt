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
import urllib
import zipfile

import gyp_utils

ANDROID_HOME = os.environ.get('ANDROID_HOME')
ANDROID_NDK_HOME = os.environ.get('ANDROID_NDK_HOME')
ANDROID_API_LEVEL = '21'

NDK_ZIP_REVISION = 'android-ndk-r13b'
NDK_ZIP_FILE = ('%s-%s-x86_64.zip' %
                (NDK_ZIP_REVISION, platform.system().lower()))
NDK_URL = 'https://dl.google.com/android/repository/%s' % NDK_ZIP_FILE

SCRIPT_CTIME = os.path.getctime(__file__)
COBALT_TOOLCHAINS_DIR = gyp_utils.GetToolchainsDir()

# Prefer a developer-installed NDK if available
if ANDROID_NDK_HOME:
  NDK_PATH = ANDROID_NDK_HOME
elif ANDROID_HOME:
  NDK_PATH = os.path.join(ANDROID_HOME, 'ndk-bundle')
else:
  NDK_PATH = os.path.join(COBALT_TOOLCHAINS_DIR, NDK_ZIP_REVISION)

NDK_PROPERTIES_PATH = os.path.join(NDK_PATH, 'source.properties')


def GetToolsPath(abi):
  """Returns the path where the NDK standalone toolchain should be."""
  tools_dir = 'android_toolchain_api%s_%s' % (ANDROID_API_LEVEL, abi)
  return os.path.realpath(os.path.join(COBALT_TOOLCHAINS_DIR, tools_dir))


def GetEnvironmentVariables(abi):
  """Returns a dictionary of environment variables to provide to GYP."""
  tools_bin = os.path.join(GetToolsPath(abi), 'bin')
  return {
      'CC': os.path.join(tools_bin, 'clang'),
      'CXX': os.path.join(tools_bin, 'clang++'),
  }


def UnzipFile(zip_path, dest_path):
  """Extract all files and restore permissions from a zip file."""
  zip_file = zipfile.ZipFile(zip_path)
  for info in zip_file.infolist():
    zip_file.extract(info.filename, dest_path)
    os.chmod(os.path.join(dest_path, info.filename), info.external_attr >> 16L)


def GetRevision(properties_path):
  """Returns the revision from a properties file."""
  ini_str = '[properties]\n' + open(properties_path, 'r').read()
  config = ConfigParser.RawConfigParser()
  config.readfp(StringIO.StringIO(ini_str))
  return config.get('properties', 'pkg.revision')


def CheckStamp(dir_path):
  """Checks that the specified directory is up-to-date with the NDK."""
  stamp_path = os.path.join(dir_path, 'ndk.stamp')
  return (os.path.exists(stamp_path) and
          os.path.getctime(stamp_path) > SCRIPT_CTIME and
          GetRevision(stamp_path) == GetRevision(NDK_PROPERTIES_PATH))


def UpdateStamp(dir_path):
  """Updates the stamp file in the specified directory to the NDK revision."""
  stamp_path = os.path.join(dir_path, 'ndk.stamp')
  shutil.copyfile(NDK_PROPERTIES_PATH, stamp_path)


def CheckNdkVersion(abi):
  """Check the installed NDK standalone tools, and update them if needed."""

  # Download the NDK if not developer-installed and it's not already downloaded
  if not ANDROID_HOME and not ANDROID_NDK_HOME:
    # Hold an exclusive advisory lock on the COBALT_TOOLCHAINS_DIR, to
    # prevent issues with modification for multiple variants.
    toolchains_dir_fd = os.open(COBALT_TOOLCHAINS_DIR, os.O_RDONLY)
    fcntl.flock(toolchains_dir_fd, fcntl.LOCK_EX)

    if not CheckStamp(NDK_PATH):
      logging.warning(
          'NDK not found in either ANDROID_HOME nor ANDROID_NDK_HOME')
      logging.warning('Downloading NDK from %s', NDK_URL)
      dl_file, dummy_headers = urllib.urlretrieve(NDK_URL)
      logging.warning('Unzipping NDK to %s', NDK_PATH)
      if os.path.exists(NDK_PATH):
        shutil.rmtree(NDK_PATH)
      # Unzip to COBALT_TOOLCHAINS_DIR since NDK_ZIP_REVISION directory is
      # in the zip archive
      UnzipFile(dl_file, COBALT_TOOLCHAINS_DIR)
      UpdateStamp(NDK_PATH)

    fcntl.flock(toolchains_dir_fd, fcntl.LOCK_UN)
    os.close(toolchains_dir_fd)

  tools_path = GetToolsPath(abi)
  if CheckStamp(tools_path):
    logging.info('NDK %s toolchain already at %s', abi,
                 GetRevision(NDK_PROPERTIES_PATH))
    return

  logging.warning('Installing NDK %s toolchain %s in %s', abi,
                  GetRevision(NDK_PROPERTIES_PATH), tools_path)

  if os.path.exists(tools_path):
    shutil.rmtree(tools_path)

  # Run the NDK script to make the standalone toolchain
  script_path = os.path.join(NDK_PATH, 'build', 'tools',
                             'make_standalone_toolchain.py')
  args = [
      script_path, '--arch', abi, '--api', ANDROID_API_LEVEL, '--install-dir',
      tools_path
  ]
  script_proc = subprocess.Popen(args)
  rc = script_proc.wait()
  if rc != 0:
    raise RuntimeError('%s failed.' % script_path)

  UpdateStamp(tools_path)

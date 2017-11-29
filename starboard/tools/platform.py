#
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
#
"""Functionality to enumerate and represent starboard ports."""

import logging
import os
import re

import _env  # pylint: disable=unused-import
from starboard.tools import environment

# The list of files that must be present in a directory to allow it to be
# considered a valid platform.
_PLATFORM_FILES = [
    'gyp_configuration.gypi',
    'gyp_configuration.py',
    'starboard_platform.gyp',
    'configuration_public.h',
    'atomic_public.h',
    'thread_types_public.h',
]


# Whether to emit warnings if it finds a directory that almost has a platform.
# TODO: Enable when tree is clean.
_WARN_ON_ALMOST = False


def _IsValidPlatformFilenameList(directory, filenames):
  """Determines if |filenames| contains the required files for a valid port."""
  missing = set(_PLATFORM_FILES) - set(filenames)
  if missing:
    if len(missing) < (len(_PLATFORM_FILES) / 2) and _WARN_ON_ALMOST:
      logging.warning('Directory %s contains several files needed for a '
                      'platform, but not all of them. In particular, it is '
                      'missing: %s', directory, ', '.join(missing))
  return not missing


def _GetPlatformName(root, directory):
  """Gets the name of a platform found at |directory| off of |root|."""
  assert directory.startswith(root)
  start = len(root) + 1  # Remove the trailing slash from the root.

  assert start < len(directory)

  # Calculate the name based on relative path from search root to directory.
  return re.sub(r'[^a-zA-Z0-9_]', r'-', directory[start:])


def _EnumeratePlatforms(root_path, exclusion_set=None):
  """Generator that iterates over Starboard platforms found under |path|."""
  if not exclusion_set:
    exclusion_set = set()

  for current_path, directories, filenames in os.walk(root_path):
    # Don't walk into any directories in the exclusion set.
    directories[:] = (x for x in directories
                      if os.path.join(current_path, x) not in exclusion_set)

    # Determine if the current directory is a valid port directory.
    if _IsValidPlatformFilenameList(current_path, filenames):
      if current_path == root_path:
        logging.warning('Found platform at search path root: %s', current_path)
      name = _GetPlatformName(root_path, current_path)
      yield PlatformInfo(name, current_path)


def _FindAllPlatforms():
  """Search the filesystem for all valid Starboard platforms.

  Returns:
    A Mapping of name->PlatformInfo.
  """

  result = {}
  search_path = environment.GetStarboardPortRoots()

  # Ignore search path directories inside other search path directories.
  exclusion_set = set(search_path)

  for entry in search_path:
    if not os.path.isdir(entry):
      continue
    for platform_info in _EnumeratePlatforms(entry, exclusion_set):
      if platform_info.name in result:
        logging.error('Found duplicate port name "%s" at "%s" and "%s"',
                      platform_info.name, result[platform_info.name],
                      platform_info.path)
      result[platform_info.name] = platform_info

  return result


# Cache of name->PlatformInfo mapping.
_INFO_MAP = None


def _GetInfoMap():
  """Gets mapping of platform names to PlatformInfo objects."""
  global _INFO_MAP
  if not _INFO_MAP:
    _INFO_MAP = _FindAllPlatforms()
  return _INFO_MAP


# Cache of the sorted list of all platform names.
_ALL = None


def GetAll():
  """Gets a sorted list of all valid Starboard platform names."""
  global _ALL
  if not _ALL:
    _ALL = sorted(_GetInfoMap().keys())
  return _ALL


def Get(platform_name):
  """Gets the PlatformInfo for the given platform name, or None."""
  return _GetInfoMap().get(platform_name)


def GetAllInfos():
  """Gets a sorted sequence of PlatformInfo objects for all valid platforms."""
  return (Get(p) for p in GetAll())


def IsValid(platform_name):
  """Determines whether the given platform name is valid."""
  return platform_name in _GetInfoMap()


class PlatformInfo(object):
  """Information about a specific Starboard platform."""

  def __init__(self, name, path):
    self.name = name
    self.path = path

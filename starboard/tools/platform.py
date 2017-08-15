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

import importlib
import logging
import os
import re


# The list of files that must be present in a directory to allow it to be
# considered a valid port.
_PORT_FILES = [
    'gyp_configuration.gypi',
    'gyp_configuration.py',
    'starboard_platform.gyp',
    'configuration_public.h',
    'atomic_public.h',
    'thread_types_public.h',
]


# Whether to emit warnings if it finds a directory that almost has a port.
# TODO: Enable when tree is clean.
_WARN_ON_ALMOST_PORTS = False


def _IsValidPortFilenameList(directory, filenames):
  """Determines if |filenames| contains the required files for a valid port."""
  missing = set(_PORT_FILES) - set(filenames)
  if missing:
    if len(missing) < (len(_PORT_FILES) / 2) and _WARN_ON_ALMOST_PORTS:
      logging.warning('Directory %s contains several files needed for a port, '
                      'but not all of them. In particular, it is missing: %s',
                      directory, ', '.join(missing))
  return not missing


def _GetPortName(root, directory):
  """Gets the name of a port found at |directory| off of |root|."""

  assert directory.startswith(root)
  start = len(root) + 1  # Remove the trailing slash from the root.

  assert start < len(directory)

  # Calculate the name based on relative path from search root to port.
  return re.sub(r'[^a-zA-Z0-9_]', r'-', directory[start:])


class PlatformInfo(object):
  """Information about a specific starboard port."""

  @classmethod
  def EnumeratePorts(cls, root_path, exclusion_set = None):
    """Generator that iterates over starboard ports found under |path|."""
    if not exclusion_set:
      exclusion_set = set()
    for current_path, directories, filenames in os.walk(root_path):
      # Don't walk into any directories in the exclusion set.
      directories[:] = (x for x in directories
                        if os.path.join(current_path, x) not in exclusion_set)
      # Determine if the current directory is a valid port directory.
      if _IsValidPortFilenameList(current_path, filenames):
        if current_path == root_path:
          logging.warning('Found port at search path root: %s', current_path)
        port_name = _GetPortName(root_path, current_path)
        yield PlatformInfo(port_name, current_path)

  def __init__(self, name, path):
    self.port_name = name
    self.path = path

  def ImportModule(self, root_module, module_name=None):
    """Load a platform specific python module using importlib.

    Load the python module named |module_name| relative to |root_module|.
    Args:
      module: Name of a python module to load. If None, load the platform
          directory as a python module.
      root_module: An already-loaded module
    Returns:
      A module loaded with importlib.import_module
    Throws:
      ImportError if the module fails to be loaded.
    """
    # From the relative path to the |root_module|'s directory, construct a full
    # python package name and attempt to load it.
    relative_path = os.path.relpath(
        self.path, os.path.dirname(root_module.__file__))
    components = os.path.normpath(relative_path).split(os.sep)
    components = [root_module.__name__] + components
    if module_name:
      components.append(module_name)
    full_package_name = '.'.join(components)
    return importlib.import_module(full_package_name)

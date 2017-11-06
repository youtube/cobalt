#
# Copyright 2017 Google Inc. All Rights Reserved.
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
# limitations under the License."""
"""Sets up a Starboard environment for use by the host application.

Applications that use Starboard should contain a Python file, called
"starboard_configuration.py", somewhere in the ancestor path of their
"starboard" directory.

The file should contain a variable, called PORT_ROOTS. Assign to it a
list of paths to directories in which Starboard ports are located.
Each path should be represented as a list of directory names whose locations
are relative to the configuration file.  Example:

PORT_ROOTS = [
    ["some_directory", "starboard"]
    ["somewhere_else", "starboard"]
]

It should also contain another variable, called APPLICATIONS.  This variable
should be a Python Mapping containing names of applications you want to support
mapped to functions that will construct their ApplicationConfiguration subclass
with all appropriate discovery and fallback logic.

Example:

APPLICATIONS = {
    'cobalt': cobalt.build.CreateCobaltConfiguration,
}

IF YOU ARE GOING TO USE STARBOARD FILES OUTSIDE OF STARBOARD, YOU NEED TO IMPORT
THIS MODULE FIRST.  Otherwise, sys.path will not be configured properly, none of
your code will work, and you'll be sad.
"""

import importlib
import os
import sys

PATHS = [
    # Necessary because the gyp_configuration for linux-x64x11 imports
    # directly from "starboard/".
    os.path.abspath(os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir)),

    # Necessary because gyp_configuration modules use relative imports.
    # "cobalt/build" needs to be in sys.path to keep the imports working
    os.path.abspath(os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir,
        "cobalt", "build"))
]

# This module should only add the above paths to sys.path when they are not
# present in the list already.
for new_path in PATHS:
  if new_path not in sys.path:
    sys.path.append(new_path)

# Saved in global state so we only ever search for it once.
_CONFIG_PATH = None


def _GetStarboardConfigPath():
  """Gets the path to the "starboard_configuration.py" file from the host app.

  Returns:
    Path to the "starboard_configuration.py" file from the host app

  Raises:
    RuntimeError: There is no configuration file.
  """
  global _CONFIG_PATH

  if _CONFIG_PATH:
    return _CONFIG_PATH

  else:
    current_path = os.path.normpath(os.path.dirname(__file__))
    while not os.path.exists(os.path.join(current_path,
                                          "starboard_configuration.py")):
      next_path = os.path.dirname(current_path)
      if next_path == current_path:
        current_path = None
        break
      current_path = next_path
    config_path = current_path

    if not config_path:
      raise RuntimeError("No starboard configuration declared.")

    _CONFIG_PATH = config_path
    return config_path


def _ImportConfigModule():
  if "starboard_configuration" in sys.modules:
    return sys.modules["starboard_configuration"]
  else:
    config_path = _GetStarboardConfigPath()
    if config_path not in sys.path:
      sys.path.append(config_path)
    return importlib.import_module("starboard_configuration")


def GetStarboardPortRoots():
  """Gets paths to the host app's Starboard port directories."""
  config_path = _GetStarboardConfigPath()
  config_module = _ImportConfigModule()
  port_roots = config_module.PORT_ROOTS
  port_root_paths = [os.path.abspath(os.path.join(
      config_path, os.sep.join(root))) for root in port_roots]
  return port_root_paths


def GetApplicationClass(application_name):
  """Gets the class or function that constructs the ApplicationConfiguration."""
  config_module = _ImportConfigModule()
  return config_module.APPLICATIONS.get(application_name)

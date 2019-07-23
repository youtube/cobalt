#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Goma specific definitions and helper functions.

Goma is used for faster builds.  Install goma and add directory to PATH.
Provides common functions to setup goma across platform builds.
It checks that goma is enabled and installed, and starts the
goma proxy.

Override settings using environment variables.
USE_GOMA  To enable/disable goma. Default is None.
"""

import logging
import os
import subprocess
import sys
import util


def _GomaEnabledFromEnv():
  """Enable goma if USE_GOMA is defined.

  Returns:
    True to enable goma, defaults to False.
  """
  if 'USE_GOMA' in os.environ:
    return os.environ['USE_GOMA'] == '1'
  return False


def _GetGomaFromPath():
  """Returns goma directory from PATH, otherwise is None."""
  gomacc_path = util.Which('gomacc')
  if gomacc_path is not None:
    return os.path.dirname(gomacc_path)
  return None


def _GomaInstalled(goma_dir):
  """Returns True if goma is installed, otherwise is False."""
  if goma_dir and os.path.isdir(goma_dir):
    if os.path.isfile(GomaControlFile(goma_dir)):
      logging.error('Using Goma installed at: %s', goma_dir)
      return True
  logging.error('Failed to find goma dir. Check PATH location: %s', goma_dir)
  return False


def GomaControlFile(goma_dir):
  """Returns path to goma control script."""
  return os.path.join(goma_dir, 'goma_ctl.py')


def _GomaEnsureStart(goma_dir):
  """Starts the goma proxy.

  Checks the proxy status and tries to start if not running.

  Args:
    goma_dir: goma install directory

  Returns:
    True if goma started, otherwise False.
  """
  if not _GomaInstalled(goma_dir):
    return False

  goma_ctl = GomaControlFile(goma_dir)
  command = [goma_ctl, 'ensure_start']
  logging.error('starting goma proxy...')

  try:
    subprocess.check_call(command)
    return True
  except subprocess.CalledProcessError as e:
    logging.error('Goma proxy failed to start.\nCommand: %s\n%s',
                  ' '.join(e.cmd), e.output)
    return False


def FindAndStartGoma(enable_in_path=True, exit_on_failed_start=False):
  """Uses goma if installed and proxy is running.

  Args:
    enable_in_path: If True, enable goma if found in PATH. Otherwise,
                    it enables it when USE_GOMA=1 is set.
    exit_on_failed_start: Boolean to exit if goma is enabled, but wasn't
                            started.
  Returns:
    True if goma is enabled and running, otherwise False.
  """
  if enable_in_path or _GomaEnabledFromEnv():
    goma_dir = _GetGomaFromPath()
    if _GomaEnsureStart(goma_dir):
      return True
    else:
      logging.critical('goma was enabled, but failed to start.')
      if exit_on_failed_start:
        sys.exit(1)
      return False
  else:
    logging.info('Goma is disabled. To enable, check for gomacc in PATH '
                 'and/or set USE_GOMA=1.')
    return False

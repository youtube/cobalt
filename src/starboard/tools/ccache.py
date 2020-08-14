#
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Ccache specific definitions and helper functions.

Override settings using environment variables.
USE_CCACHE  To enable/disable ccache. Default is None.
"""

import logging
import os
import util

from starboard.tools import build_accelerator


class Ccache(build_accelerator.BuildAccelerator):
  """Ccache is a cache based build accelerator."""

  def GetName(self):
    return 'ccache'

  def Use(self):
    return CcacheEnvOverride() and CcacheInstalled()


def CcacheEnvOverride():
  """Checks USE_CCACHE to enable/disable ccache.

  Returns:
    USE_CCACHE boolean if exists, defaults to True.
  """
  if 'USE_CCACHE' in os.environ:
    return os.environ['USE_CCACHE'] == '1'
  return True


def CcacheInstalled():
  """Returns True if ccache is installed, otherwise is False."""
  ccache_path = util.Which('ccache')
  if ccache_path is not None:
    logging.info('Using ccache installed at: %s', ccache_path)
    return True
  logging.error('Unable to find ccache.')
  return False

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
"""Cache specific definitions and helper functions.

Override settings to enable/disable cache using environment variables. Default
is None.
"""

import logging
import os

from starboard.tools import build_accelerator
from starboard.tools import util


class Accelerator:
  CCACHE = ('ccache', 'USE_CCACHE')
  # Windows compatible ccache.
  SCCACHE = ('sccache.exe', 'USE_SCCACHE')


class Cache(build_accelerator.BuildAccelerator):
  """Cache based build accelerator."""

  def __init__(self, accelerator):
    self.binary, self.env_var = accelerator

  def GetName(self):
    return self.binary

  def Use(self):
    return self.EnvOverride() and self.BinaryInstalled()

  def EnvOverride(self):
    """Checks env_var to enable/disable cache.

    Returns:
      env_var boolean if exists, defaults to True.
    """
    if self.env_var in os.environ:
      return os.environ[self.env_var] == '1'
    return True

  def BinaryInstalled(self):
    """Returns True if binary is installed, otherwise is False."""
    binary_path = util.Which(self.binary)
    if binary_path is not None:
      logging.info('Using %s installed at: %s', self.binary, binary_path)
      return True
    logging.info('Unable to find %s.', self.binary)
    return False

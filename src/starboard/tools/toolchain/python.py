# Copyright 2018 Google Inc. All Rights Reserved.
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
"""Allow use of gyp-win-tool via python as copy and stamp tools."""

import sys
from starboard.tools.toolchain import abstract


class Copy(abstract.Copy):
  """Copies individual files."""

  def __init__(self, **kwargs):
    self._path = kwargs.get('path', sys.executable)
    self._extra_flags = kwargs.get('extra_flags',
                                   ['gyp-win-tool', 'recursive-mirror'])

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    return self._extra_flags

  def GetMaxConcurrentProcesses(self):
    # Run as much concurrent processes as possible.
    return None

  def GetCommand(self, path, extra_flags, flags, shell):
    del flags, shell  # Not used.
    return '{0} {1} $in $out'.format(path, extra_flags)

  def GetDescription(self):
    return 'COPY $in $out'

  def GetRspFilePath(self):
    # A command line only contains one input and one output file.
    pass

  def GetRspFileContent(self):
    # A command line only contains one input and one output file.
    pass


class Stamp(abstract.Stamp):
  """Updates the access and modification times of a file to the current time."""

  def __init__(self, **kwargs):
    self._path = kwargs.get('path', sys.executable)
    self._extra_flags = kwargs.get('extra_flags', ['gyp-win-tool', 'stamp'])

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    return self._extra_flags

  def GetMaxConcurrentProcesses(self):
    # Run as much concurrent processes as possible.
    return None

  def GetCommand(self, path, extra_flags, flags, shell):
    del flags, shell  # Not used.
    return '{0} {1} $out'.format(path, extra_flags)

  def GetDescription(self):
    return 'STAMP $out'

  def GetRspFilePath(self):
    # A command line only contains one input file.
    pass

  def GetRspFileContent(self):
    # A command line only contains one input file.
    pass

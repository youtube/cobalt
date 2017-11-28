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
# limitations under the License.
"""Allows to use Darwin (not GNU!) libtool as a static linker."""

from starboard.tools.toolchain import abstract
from starboard.tools.toolchain import common


class StaticLinker(abstract.StaticLinker):
  """Creates self-contained archives using Darwin (not GNU!) libtool."""

  def __init__(self, **kwargs):
    self._path = common.GetPath('libtool', **kwargs)
    self._extra_flags = kwargs.get('extra_flags', [])

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    return self._extra_flags

  def GetMaxConcurrentProcesses(self):
    # Run as much concurrent processes as possible.
    return None

  def GetCommand(self, path, extra_flags, flags, shell):
    del flags, shell  # Not used.
    return '{0} -static {1} -filelist $rspfile -o $out'.format(
        path, extra_flags)

  def GetDescription(self):
    return 'LIBTOOL $out'

  def GetRspFilePath(self):
    return '$out.rsp'

  def GetRspFileContent(self):
    return '$in_newline'

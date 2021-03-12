# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Allows to use Clang (invoked as clang++) as a dynamic linker."""

from starboard.tools.toolchain import abstract
from starboard.tools.toolchain import common


class DynamicLinkerBase(object):
  """A base class for Clang based linking of executables and shared libraries.

  Invoked as clang++.
  """

  def __init__(self, **kwargs):
    self._path = common.GetPath('clang++', **kwargs)
    self._extra_flags = kwargs.get('extra_flags', [])
    self._max_concurrent_processes = kwargs.get(
        'max_concurrent_processes', common.EstimateMaxConcurrentLinkers())

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    return self._extra_flags

  def GetMaxConcurrentProcesses(self):
    return self._max_concurrent_processes

  def GetDescription(self):
    return 'LINK $out'

  def GetRspFilePath(self):
    return '$out.rsp'

  def GetRspFileContent(self):
    return '$in_newline'

  def GetFlags(self, ldflags):
    return ldflags


class ExecutableLinker(DynamicLinkerBase, abstract.ExecutableLinker):
  """Links executables using Clang (invoked as clang++)."""

  def __init__(self, **kwargs):
    super(ExecutableLinker, self).__init__(**kwargs)
    # Groups archives to be searched until all references are resolved.
    self._write_group = kwargs.get('write_group', False)

  def GetCommand(self, path, extra_flags, flags, shell):
    del shell  # Not used.
    if self._write_group:
      return ('{0} {1} -Wl,--start-group @$rspfile -Wl,--end-group -o $out {2}'
              .format(path, extra_flags, flags))
    else:
      return '{0} {1} @$rspfile {2} -o $out'.format(path, extra_flags, flags)


class SharedLibraryLinker(DynamicLinkerBase, abstract.SharedLibraryLinker):
  """Links shared libraries using Clang (invoked as clang++)."""

  def __init__(self, **kwargs):
    super(SharedLibraryLinker, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    del shell  # Not used.
    return ('{0} -shared {1} -o $out {2} -Wl,-soname=$soname '
            '-Wl,--whole-archive @$rspfile -Wl,--no-whole-archive').format(
                path, extra_flags, flags)

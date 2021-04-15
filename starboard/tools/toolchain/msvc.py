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
"""Allows use of MSVC build tools."""

import sys

from starboard.tools.toolchain import abstract
from starboard.tools.toolchain import common


def QuoteArguments(args):
  return ['\"{0}\"'.format(arg.replace('"', '\\"')) for arg in args]


class CompilerBase(object):
  """A base class for MSVC-based compilers."""

  def __init__(self, **kwargs):
    self._path = common.GetPath('msvc', **kwargs)
    self._gyp_defines = kwargs.get('gyp_defines', [])
    self._gyp_include_dirs = kwargs.get('gyp_include_dirs', [])
    self._gyp_cflags = kwargs.get('gyp_cflags', [])

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    # Not used.
    return []

  def GetMaxConcurrentProcesses(self):
    # Run as many concurrent processes as possible.
    return None

  def GetHeaderDependenciesFilePath(self):
    pass

  def GetHeaderDependenciesFormat(self):
    return 'msvc'

  def GetRspFilePath(self):
    return None

  def GetRspFileContent(self):
    return None

  def GetCommand(self, path, extra_flags, flags, shell):
    del extra_flags  # Not used.
    del shell  # Not used.
    return ('{path} /nologo /showIncludes {flags} /c $in /Fo$out /Fd$out.pdb'
            .format(path=path, flags=flags))

  def GetFlags(self, defines, include_dirs, cflags):
    defines = defines + self._gyp_defines
    quoted_defines = QuoteArguments(defines)
    define_flags = ['/D{0}'.format(define) for define in quoted_defines]
    include_dirs = include_dirs + self._gyp_include_dirs
    quoted_include_dirs = QuoteArguments(include_dirs)
    include_dir_flags = [
        '/I{0}'.format(include_dir) for include_dir in quoted_include_dirs
    ]
    if self._gyp_cflags:
      cflags = self._gyp_cflags
    return define_flags + include_dir_flags + cflags


class CCompiler(CompilerBase, abstract.CCompiler):
  """Compiles C sources using MSVC."""

  def GetDescription(self):
    return 'CC $out'


class CxxCompiler(CompilerBase, abstract.CxxCompiler):
  """Compiles C++ sources using MSVC."""

  def GetDescription(self):
    return 'CXX $out'


class AssemblerWithCPreprocessor(abstract.AssemblerWithCPreprocessor):
  """Compiles assembler sources that contain C preprocessor directives."""

  def __init__(self, **kwargs):
    self._path = common.GetPath('masm', **kwargs)
    self._arch = kwargs.get('arch', 'environment.x64')
    self._gyp_defines = kwargs.get('gyp_defines', [])
    self._gyp_include_dirs = kwargs.get('gyp_include_dirs', [])

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    # Not used.
    return []

  def GetMaxConcurrentProcesses(self):
    # Run as much concurrent processes as possible.
    return None

  def GetHeaderDependenciesFilePath(self):
    pass

  def GetHeaderDependenciesFormat(self):
    pass

  def GetRspFilePath(self):
    pass

  def GetRspFileContent(self):
    pass

  def GetCommand(self, path, extra_flags, flags, shell):
    del extra_flags  # Not used.
    del shell  # Not used.
    return ('{python} gyp-win-tool asm-wrapper {arch} {path} {flags} /c /Fo '
            '$out $in'.format(
                python=sys.executable, arch=self._arch, path=path, flags=flags))

  def GetDescription(self):
    return 'ASM $in'

  def GetFlags(self, defines, include_dirs, cflags):
    return []


class StaticLinkerBase(object):
  """A base class for MSVC-based static linkers."""

  def __init__(self, **kwargs):
    self._path = common.GetPath('ar', **kwargs)
    self._arch = kwargs.get('arch', 'environment.x64')
    self._gyp_libflags = kwargs.get('gyp_libflags', [])
    self._max_concurrent_processes = kwargs.get('max_concurrent_processes',
                                                None)

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    # Not used.
    return []

  def GetMaxConcurrentProcesses(self):
    return self._max_concurrent_processes

  def GetDescription(self):
    return 'LIB $out'

  def GetRspFilePath(self):
    return '$out.rsp'

  def GetRspFileContent(self):
    return '$in_newline ' + self._command_flags

  def GetCommand(self, path, extra_flags, flags, shell):
    del extra_flags  # Not used.
    del shell  # Not used.
    self._command_flags = flags
    return ('{python} gyp-win-tool link-wrapper {arch} {path} /nologo '
            '/ignore:4221 /OUT:$out @$out.rsp'.format(
                python=sys.executable, arch=self._arch, path=path))

  def GetFlags(self):
    return self._gyp_libflags


class StaticLinker(StaticLinkerBase, abstract.StaticLinker):
  """Creates self-contained archives using LIB.exe."""

  def __init__(self, **kwargs):
    super(StaticLinker, self).__init__(**kwargs)


class StaticThinLinker(StaticLinkerBase, abstract.StaticThinLinker):
  """Creates thin archives using LIB.exe."""

  def __init__(self, **kwargs):
    super(StaticThinLinker, self).__init__(**kwargs)


class DynamicLinkerBase(object):
  """A base class for MSVC-based executable and shared library linkers."""

  def __init__(self, **kwargs):
    self._path = common.GetPath('ld', **kwargs)
    self._max_concurrent_processes = kwargs.get(
        'max_concurrent_processes', common.EstimateMaxConcurrentLinkers())
    self._arch = kwargs.get('arch', 'environment.x64')
    self._gyp_ldflags = kwargs.get('gyp_ldflags', [])

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    # Not used.
    return []

  def GetMaxConcurrentProcesses(self):
    return self._max_concurrent_processes

  def GetFlags(self, ldflags):
    del ldflags  # Not used.
    return self._gyp_ldflags


class ExecutableLinker(DynamicLinkerBase, abstract.ExecutableLinker):
  """Links executables using LINK.exe."""

  def __init__(self, **kwargs):
    super(ExecutableLinker, self).__init__(**kwargs)

  def GetDescription(self):
    return 'LINK $out'

  def GetRspFilePath(self):
    return '$out.rsp'

  def GetRspFileContent(self):
    return '$in_newline ' + self._command_flags

  def GetCommand(self, path, extra_flags, flags, shell):
    del extra_flags  # Not used.
    del shell  # Not used.
    self._command_flags = flags
    return ('{python} gyp-win-tool link-wrapper {arch} {path} /nologo '
            '/OUT:$out /PDB:$out.pdb @$out.rsp'.format(
                python=sys.executable, arch=self._arch, path=path))


class SharedLibraryLinker(DynamicLinkerBase, abstract.SharedLibraryLinker):
  """Links shared libraries using LINK.exe."""

  def __init__(self, **kwargs):
    super(SharedLibraryLinker, self).__init__(**kwargs)

  def GetDescription(self):
    return 'LINK(DLL) $dll'

  def GetRspFilePath(self):
    return '$dll.rsp'

  def GetRspFileContent(self):
    return '$in_newline ' + self._command_flags

  def GetCommand(self, path, extra_flags, flags, shell):
    del extra_flags  # Not used.
    del shell  # Not used.
    self._command_flags = flags
    return ('{python} gyp-win-tool link-wrapper {arch} {path} /nologo '
            '$implibflag /DLL /OUT:$dll /PDB:$dll.pdb @$dll.rsp'.format(
                python=sys.executable, arch=self._arch, path=path))

  def GetRestat(self):
    return True

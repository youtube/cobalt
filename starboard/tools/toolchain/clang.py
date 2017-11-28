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
"""Allows to use Clang as a compiler and assembler."""

from starboard.tools.toolchain import abstract
from starboard.tools.toolchain import common


class CompilerBase(object):
  """A base class for Clang-based compilers and assemblers."""

  def __init__(self, **kwargs):
    self._path = common.GetPath('clang', **kwargs)
    self._extra_flags = kwargs.get('extra_flags', [])

  def GetPath(self):
    return self._path

  def GetExtraFlags(self):
    return self._extra_flags

  def GetMaxConcurrentProcesses(self):
    # Run as much concurrent processes as possible.
    return None

  def GetHeaderDependenciesFilePath(self):
    return '$out.d'

  def GetHeaderDependenciesFormat(self):
    return 'gcc'

  def GetRspFilePath(self):
    # A command line only contains one input and one output file.
    pass

  def GetRspFileContent(self):
    # A command line only contains one input and one output file.
    pass


def _GetClangCommand(path, language, extra_flags, flags, shell):
  del shell  # Not used.
  return ('{path} -x {language} -MMD -MF $out.d {extra_flags} {flags} -c $in '
          '-o $out'.format(path=path, language=language,
                           extra_flags=extra_flags, flags=flags))


class CCompiler(CompilerBase, abstract.CCompiler):
  """Compiles C sources using Clang."""

  def __init__(self, **kwargs):
    super(CCompiler, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    return _GetClangCommand(path, 'c', extra_flags, flags, shell)

  def GetDescription(self):
    return 'CC $out'

  def GetFlags(self, defines, include_dirs, cflags):
    define_flags = ['-D{0}'.format(define) for define in defines]
    include_dir_flags = [
        '-I{0}'.format(include_dir) for include_dir in include_dirs
    ]
    return define_flags + include_dir_flags + cflags


class CxxCompiler(CompilerBase, abstract.CxxCompiler):
  """Compiles C++ sources using Clang."""

  def __init__(self, **kwargs):
    super(CxxCompiler, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    return _GetClangCommand(path, 'c++', extra_flags, flags, shell)

  def GetDescription(self):
    return 'CXX $out'

  def GetFlags(self, defines, include_dirs, cflags):
    define_flags = ['-D{0}'.format(define) for define in defines]
    include_dir_flags = [
        '-I{0}'.format(include_dir) for include_dir in include_dirs
    ]
    return define_flags + include_dir_flags + cflags


class ObjectiveCxxCompiler(CompilerBase, abstract.ObjectiveCxxCompiler):
  """Compiles Objective-C++ sources using Clang."""

  def __init__(self, **kwargs):
    super(ObjectiveCxxCompiler, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    return _GetClangCommand(path, 'objective-c++', extra_flags, flags, shell)

  def GetDescription(self):
    return 'OBJCXX $out'

  def GetFlags(self, defines, include_dirs, cflags):
    define_flags = ['-D{0}'.format(define) for define in defines]
    include_dir_flags = [
        '-I{0}'.format(include_dir) for include_dir in include_dirs
    ]
    return define_flags + include_dir_flags + cflags


class AssemblerWithCPreprocessor(CompilerBase,
                                 abstract.AssemblerWithCPreprocessor):
  """Compiles assembler sources that contain C preprocessor directives."""

  def __init__(self, **kwargs):
    super(AssemblerWithCPreprocessor, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    return _GetClangCommand(path, 'assembler-with-cpp', extra_flags, flags,
                            shell)

  def GetDescription(self):
    return 'ASM $out'

  def GetFlags(self, defines, include_dirs, cflags):
    define_flags = ['-D{0}'.format(define) for define in defines]
    include_dir_flags = [
        '-I{0}'.format(include_dir) for include_dir in include_dirs
    ]
    return define_flags + include_dir_flags + cflags

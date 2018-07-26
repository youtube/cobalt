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
"""Allows to use ar and clang++ as static and dynamic linkers.

Inherited the linker definitions from ar.py and clangxx.py. Changed the input
to the linkers to be $in instead of @$rspfile, because when the rspfile is
passed to the linkers, the file content, which is the paths to the object files,
is not quoted. As a result, the file paths cannot be interpreted correctly by
the linkers. When $in is passed to the linkers, each file path is quoted and
interpreted correctly.
"""

from starboard.tools.toolchain import abstract
from starboard.tools.toolchain import ar
from starboard.tools.toolchain import clangxx


class StaticLinker(ar.StaticLinkerBase, abstract.StaticLinker):
  """Creates self-contained archives using GNU ar."""

  def __init__(self, **kwargs):  # pylint: disable=useless-super-delegation
    super(StaticLinker, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    del flags  # Not used.
    return '{0} rcs {1} $out $in'.format(path, extra_flags)


class StaticThinLinker(ar.StaticLinkerBase, abstract.StaticThinLinker):
  """Creates thin archives using GNU ar."""

  def __init__(self, **kwargs):  # pylint: disable=useless-super-delegation
    super(StaticThinLinker, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    del flags  # Not used.
    return '{0} rcsT {1} $out $in'.format(path, extra_flags)


class ExecutableLinker(clangxx.DynamicLinkerBase, abstract.ExecutableLinker):
  """Links executables using Clang (invoked as clang++)."""

  def __init__(self, **kwargs):  # pylint: disable=useless-super-delegation
    super(ExecutableLinker, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    del shell  # Not used.
    return '{0} {1} {2} $in -o $out'.format(path, extra_flags, flags)


class SharedLibraryLinker(clangxx.DynamicLinkerBase,
                          abstract.SharedLibraryLinker):
  """Links shared libraries using Clang (invoked as clang++)."""

  def __init__(self, **kwargs):  # pylint: disable=useless-super-delegation
    super(SharedLibraryLinker, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    del shell  # Not used.
    return ('{0} -shared {1} -o $out {2} -Wl,-soname=$soname '
            '-Wl,--whole-archive $in -Wl,--no-whole-archive').format(
                path, extra_flags, flags)

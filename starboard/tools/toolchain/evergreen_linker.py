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

from starboard.tools.toolchain import abstract
from starboard.tools.toolchain import clangxx


class SharedLibraryLinker(clangxx.DynamicLinkerBase,
                          abstract.SharedLibraryLinker):
  """Links shared libraries using the LLVM Project's lld."""

  def __init__(self, **kwargs):  # pylint: disable=useless-super-delegation
    super(SharedLibraryLinker, self).__init__(**kwargs)

  def GetCommand(self, path, extra_flags, flags, shell):
    lld_path = '{0}/bin/ld.lld'.format(self.GetPath())

    return shell.And('{0} '
                     '--build-id '
                     '-gc-sections '
                     '-X '
                     '-v '
                     '--eh-frame-hdr '
                     '--fini=__cxa_finalize '
                     '{2} '
                     '-shared '
                     '-o $out '
                     '-L{1} '
                     '-L/usr/lib '
                     '-L/lib '
                     '-soname=$soname '
                     '-nostdlib '
                     '--whole-archive '
                     '--no-whole-archive '
                     '-u GetEvergreenSabiString '
                     '@$rspfile'.format(lld_path, self.GetPath(), *extra_flags))

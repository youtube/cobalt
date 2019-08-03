#!/usr/bin/python -S
# Copyright 2012 Google Inc. All Rights Reserved.
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

# We have to fixup the deps output in a few ways.
# (1) the file output should mention the proper .o file.
# ccache or distcc lose the path to the target, so we convert a rule of
# the form:
#   foobar.o: DEP1 DEP2
# into
#   path/to/foobar.o: DEP1 DEP2
# (2) we want missing files not to cause us to fail to build.
# We want to rewrite
#   foobar.o: DEP1 DEP2 \\
#               DEP3
# to
#   DEP1:
#   DEP2:
#   DEP3:
# so if the files are missing, they're just considered phony rules.
# We have to do some pretty insane escaping to get those backslashes
# and dollar signs past make, the shell, and sed at the same time.
# Doesn't work with spaces, but that's fine: .d files have spaces in
# their names replaced with other characters.

# Additionally, this is done in Python to increase flexibility and to
# speed up the process on cygwin, where fork() is very slow and one
# script can be much faster than many small utilities chained together.

import platform
import os
import re
import sys

if sys.platform == 'cygwin':
  import cygpath
  import ntpath

def main(raw_depfile_path, processed_depfile_path, target_path):
  # it's okay if it doesn't exist.
  # maybe there were no #include directives.
  if not os.path.exists(raw_depfile_path):
    return

  raw_deps = file(raw_depfile_path).readlines()
  cwd = os.path.realpath(os.getcwd())
  processed_deps = fixup_dep(raw_deps, target_path, cwd)

  f = file(processed_depfile_path, 'w')
  f.write(processed_deps)
  f.close()

  # remove the raw file
  os.unlink(raw_depfile_path)

def cygwin_fix_dep_path(path, cwd):
  # fixes dependency paths so that they can be tracked by our system
  # makes absolute paths into relative paths if they are descendants of cwd.
  assert(sys.platform == 'cygwin')
  
  path = cygpath.to_unix(path)
  if path[:len(cwd)] == cwd:
    path = path[len(cwd)+1:]

  # Ninja targets have '\'s on Cygwin, not '/'s.
  path = ntpath.normpath(path).replace('\\', '\\\\')

  return path

def linux_fix_dep_path(p):
  return p.replace('\\', '/')

def fixup_dep(contents, target_path, cwd):
  # contents is an array of lines of text from the raw dep file.
  # we will return the processed dep file.

  # Some compilers generate each dependency on a separate line,
  # e.g.
  # target: dep0
  # target: dep1
  # So detect that.

  if len(contents) > 1 and contents[1].find(':') != -1:
    is_separate_lines = 1
  else:
    is_separate_lines = 0

  if is_separate_lines:
    first_obj, first_dep = contents[0].split(':', 1)
    deps = [first_dep.rstrip()]
    for l in contents:
      obj, dep = l.split(':', 1)

      # For now assume every target is the same.
      # If it's not, we'll have to fix this script.
      if obj != first_obj:
        raise Exception('Unexpected target {0}'.format(obj))

      deps.extend([dep.rstrip()])
  else:
    # strip newlines
    contents = [ i.rstrip() for i in contents ]

    # break the contents down into the object and its dependencies
    obj, first_dep = contents[0].split(':', 1)
    deps = [ first_dep ]
    deps.extend(contents[1:])

  # these deps still have spaces at the front and
  # (potentially) backslashes at the end
  deps = [ i.strip(' \t\\') for i in deps ]

  # some compilers cannot distinguish between system includes and local
  # includes specified with #include <...>, so we will make that call
  # based on the paths.  any absolute paths will be assumed to be system
  # includes and will be filtered out.
  deps = filter(lambda i: len(i) and i[0] != '/', deps)

  if sys.platform == 'cygwin':
    # some compilers will generate dependencies with Windows paths, which
    # cygwin's make doesn't like.  the colons confuse it.
    # Also, we need the listed dep files to match the format of paths
    # output by ninja.GypPathToNinja() so that dependency target names match up
    # between gyp/ninja and those in the dep file.
    deps = [ cygwin_fix_dep_path(i, cwd) for i in deps ]
  else:
    # This is only needed for cross-compilers that might generate
    # Windows-style paths.
    # ninja expects forwards slashes.
    deps = [ linux_fix_dep_path(i) for i in deps ]

  # now that the data is pre-processed, the formatting is easy.
  # use the target path (full) instead of what the compiler gave us in "obj",
  # which may be only the basename
  return "%s: \\\n" % target_path + \
      " \\\n".join([ " %s" % i for i in deps ]) + "\n" + \
      ":\n".join(deps) + ":\n"

main(sys.argv[1], sys.argv[2], sys.argv[3])


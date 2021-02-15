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
"""Gyp extensions, called with pymod_do_main."""
import argparse
import glob
import logging
import os
import stat
import sys


# lifted from standard lib webbrowser.py
def isexecutable(cmd):
  """Returns whether the input file is an executable."""
  if sys.platform[:3] == 'win':
    extensions = ('.exe', '.bat', '.cmd')
    cmd = cmd.lower()
    if cmd.endswith(extensions) and os.path.isfile(cmd):
      return cmd
    for ext in extensions:
      if os.path.isfile(cmd + ext):
        return cmd + ext
  else:
    if os.path.isfile(cmd):
      mode = os.stat(cmd)[stat.ST_MODE]
      if mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH):
        return cmd


class ExtensionCommandParser(argparse.ArgumentParser):
  """Helper class for parsing arguments to extended gyp functions."""

  def __init__(self, list_args):
    argparse.ArgumentParser.__init__(self)
    for arg in list_args:
      self.add_argument(arg)

  def error(self, message):
    print('Parse error in gyp_functions.py:' + message)
    raise NotImplementedError('Parse error:' + message)


class Extensions(object):
  """Container class for extended functionality for gyp.

  Supports operations such as:
    - file globbing
    - checking file or directory existence
    - string manipulations
    - retrieving environment variables
    - searching PATH and PYTHONPATH for programs.
  """

  def __init__(self, argv):
    parser = argparse.ArgumentParser()
    all_cmds = [x for x in dir(self) if not x.startswith('_')]
    parser.add_argument('command', help='Command to run', choices=all_cmds)
    args = parser.parse_args(argv[0:1])
    self.argv = argv[1:]
    self.command = args.command

  def call(self):
    return getattr(self, self.command)()

  def wrap_windows_path(self):
    """Fixup a windows-style path by dealing with separators and spaces."""
    path = os.path.normpath('/'.join(self.argv))
    ret = path.replace(os.sep, '/')
    ret = '"' + ret.strip() + '"'
    return ret

  def file_glob(self):
    """Glob files in dir, with pattern glob."""
    args = ExtensionCommandParser(['dir', 'pattern']).parse_args(self.argv)
    path = os.path.normpath(os.path.join(args.dir, args.pattern))
    ret = ''
    for f in glob.iglob(path):
      ret += f.replace(os.sep, '/') + ' '
    return ret.strip()

  def basename(self):
    """Basename of list of files"""
    parser = ExtensionCommandParser([])
    parser.add_argument('input_list', nargs='*')
    args = parser.parse_args(self.argv)
    ret = [os.path.basename(x) for x in args.input_list]
    return ' '.join(ret)

  def replace_in_list(self):
    """String replace in a list of arguments"""
    parser = ExtensionCommandParser(['old', 'new'])
    parser.add_argument('input_list', nargs='*')
    args = parser.parse_args(self.argv)
    inp = args.input_list
    return ' '.join([x.replace(args.old, args.new) for x in inp])

  def file_glob_sub(self):
    """Glob files, but return filenames with string replace from->to applied."""
    args = ExtensionCommandParser(
        ['dir', 'pattern', 'from_string', 'to_string']).parse_args(self.argv)
    path = os.path.normpath(os.path.join(args.dir, args.pattern))
    ret = ''
    for f in glob.iglob(path):
      ret += f.replace(os.sep, '/').replace(args.from_string,
                                            args.to_string) + ' '
    return ret.strip()

  def file_exists(self):
    """Checks if a file exists, returning a string '1' if so, or '0'."""
    args = ExtensionCommandParser(['file']).parse_args(self.argv)
    filepath = args.file.replace(os.sep, '/')
    ret = os.path.isfile(filepath)
    return str(int(ret))

  def dir_exists(self):
    """Checks if a directory exists, returning a string 'True' or 'False'."""
    args = ExtensionCommandParser(['dir']).parse_args(self.argv)
    return str(os.path.isdir(args.dir))

  def str_upper(self):
    """Converts an input string to upper case."""
    if self.argv:
      args = ExtensionCommandParser(['str']).parse_args(self.argv)
      return args.str.upper()
    return ''

  def find_program(self):
    """Searches for the input program name (.exe, .cmd or .bat)."""
    args = ExtensionCommandParser(['program']).parse_args(self.argv)
    paths_to_check = []
    # Collect all paths in PYTHONPATH.
    for i in sys.path:
      paths_to_check.append(i.replace(os.sep, '/'))
    # Then collect all paths in PATH.
    for i in os.environ['PATH'].split(os.pathsep):
      paths_to_check.append(i.replace(os.sep, '/'))
    # Check all the collected paths for the program.
    for path in paths_to_check:
      exe = os.path.join(path, args.program)
      prog = isexecutable(exe)
      if prog:
        return prog.replace(os.sep, '/')
    # If not in PYTHONPATH and PATH, check upwards until root.
    # Note: This is a rare case.
    root_dir = os.path.dirname(os.path.abspath(__file__))
    previous_dir = os.path.abspath(__file__)
    while root_dir and root_dir != previous_dir:
      exe = os.path.join(root_dir, args.program)
      prog = isexecutable(exe)
      if prog:
        return prog.replace(os.sep, '/')
      previous_dir = root_dir
      root_dir = os.path.dirname(root_dir)
    logging.error('Failed to find program "{}".'.format(args.program))
    return None

  def getenv(self):
    """Gets the stored value of an environment variable."""
    args = ExtensionCommandParser(['var']).parse_args(self.argv)
    value = os.getenv(args.var)
    if value is not None:
      return value.strip()
    return ''


def DoMain(argv):  # pylint: disable=invalid-name
  """Script main function."""
  return Extensions(argv).call()


if __name__ == '__main__':
  print(DoMain(sys.argv[1:]))
  sys.exit(0)

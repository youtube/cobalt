#!/usr/bin/env python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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


"""Runs a cobalt archive without any dependencies on external code."""


import argparse
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile


# pylint: disable=bad-continuation
_HELP_MSG = (
"""
Loads & Runs a Cobalt Archive.

Example 1: Prints the archive metadata.
  python cobalt_archive_runner.py <ARCHIVE.ZIP>

Example 2:
  python cobalt_archive_runner.py <ARCHIVE.ZIP> -- run_tests nplb

Example 3:
  python cobalt_archive_runner.py <ARCHIVE.ZIP> -- run cobalt

Example 4:
  python cobalt_archive_runner.py --temp_dir <temp/path> <ARCHIVE.ZIP> -- run_tests nplb

Example 5:
  python cobalt_archive_runner.py --temp_dir <temp/path> <ARCHIVE.ZIP> -- python starboard/tools/testing/test_runner.py <args...>
"""
)

# Suffix path for metadata inside the archive, unix style paths required.
_METADATA_JSON_SUFFIX = '__cobalt_archive/metadata.json'

# Use unix-style paths needed for zip and win32 seems to handle unix style paths
# just fine.
_DECOMPRESS_PY_SUFFIX = '__cobalt_archive/finalize_decompression/decompress.py'


def _Print(s):
  """Unix style end-lines."""
  sys.stdout.write(s + '\n')


def _CallShell(cmd_str, cwd=None):
  p = subprocess.Popen(cmd_str, cwd=cwd, shell=True, universal_newlines=True)
  try:
    return p.wait()
  except KeyboardInterrupt as kerr:
    p.terminate()
    raise kerr
  return 1


def _ReadMetadataFromArchive(zip_path):
  if not os.path.isfile(zip_path):
    raise IOError('%s does not exist.' % zip_path)
  with zipfile.ZipFile(zip_path, 'r', allowZip64=True) as zf:
    return json.loads(zf.read(_METADATA_JSON_SUFFIX))


def _GetBaseTempDir():
  base_dir = os.path.join(tempfile.gettempdir(), 'car_tmp')
  if not os.path.exists(base_dir):
    os.makedirs(base_dir)
  return base_dir


def _MakeOutputPath():
  base_dir = _GetBaseTempDir()
  return tempfile.mkdtemp(prefix='', dir=base_dir)


def _MetadataToPrettyPrintString(metadata):
  strings = []
  for key, val in sorted(metadata.items(), key=lambda x: x[0]):
    strings.append('  %s: %s' % (key, val))
  return '\n'.join(strings)


def _TryGetTrampoline(unpack_dir, command):
  """Returns a valid tranpoline from the given input, else None."""
  trampoline_target = os.path.join(
      unpack_dir, '__cobalt_archive', 'run', command + '.py')
  if os.path.isfile(trampoline_target):
    return trampoline_target
  else:
    return None


def _UnpackArchive(archive, unpack_dir):
  """Unpacks archive into the unpack_dir."""
  if not os.path.isfile(archive):
    raise IOError('%s does not exist.' % archive)
  unpack_dir = os.path.abspath(unpack_dir)
  _Print('Unzipping %s -> %s' % (archive, unpack_dir))
  # First pass, unzip all files.
  with zipfile.ZipFile(archive, 'r', allowZip64=True) as zf:
    for zinfo in zf.infolist():
      try:
        zf.extract(zinfo, path=unpack_dir)
      except Exception as err:  # pylint: disable=broad-except
        msg = (
          'Exception happend during extraction of %s because of error %s'
          % (zinfo.filename, err)
        )
        _Print(msg)
  # Second pass, execute the final decompress, which expands symlinks and other
  # file system operations.
  decomp_py = os.path.abspath(os.path.join(unpack_dir, _DECOMPRESS_PY_SUFFIX))
  cmd = 'python "%s"' % decomp_py
  _Print(cmd)
  if _CallShell(cmd) != 0:
    raise IOError('Failed to unzip %s' % archive)
  with open(os.path.join(unpack_dir, _METADATA_JSON_SUFFIX)) as fd:
    return json.loads(fd.read())


def _RunTrampoline(unpack_dir, trampoline, args):
  trampoline_target = _TryGetTrampoline(unpack_dir, trampoline)
  if trampoline_target is None:
    raise IOError('Trampoline %s does not exist.' % trampoline)
  if args:
    _Print('Invoking trampoline %s with args %s' % (trampoline, args))
  else:
    _Print('Invoking trampoline %s' % trampoline)
  trampoline = ['python', trampoline_target] + args
  cmd_str = ' '.join(trampoline)
  _CallShell(cmd_str, cwd=unpack_dir)


def _RunCommand(unpack_dir, command, args):
  cmd_str = ' '.join([command] + args)
  _CallShell(cmd_str, cwd=unpack_dir)


def _Clean(d):
  _Print('Deleting: {}'.format(d))
  shutil.rmtree(d)


def _ParseArgs(argv):
  """Parses the argument array and returns it as a namespace."""

  class MyParser(argparse.ArgumentParser):

    def error(self, message):
      self.print_help()
      sys.stderr.write('\n\nerror: %s\n' % message)
      sys.exit(2)
  # Enables new lines in the description and epilog.
  formatter_class = argparse.RawDescriptionHelpFormatter
  parser = MyParser(epilog=_HELP_MSG, formatter_class=formatter_class)
  parser.add_argument('archive')
  parser.add_argument('command', nargs='?')
  parser.add_argument('--temp_dir', '-t')
  parser.add_argument('--keep', '-k', action='store_true')
  parser.add_argument('command_arg', nargs='*')
  args = parser.parse_args(argv)
  if args.temp_dir:
    args.keep = True
  return args


def main():
  """Parses input executes proper actions."""
  fmt = '[%(filename)s:%(lineno)s:%(levelname)s] %(message)s'
  logging.basicConfig(format=fmt, level=logging.INFO)
  args = _ParseArgs(sys.argv[1:])
  if args.archive:
    metadata = _ReadMetadataFromArchive(args.archive)
    metadata_str = _MetadataToPrettyPrintString(metadata)
    _Print('Archive %s\n%s' % (args.archive, metadata_str))
  cleanup_fcn = []
  if args.command:
    if args.temp_dir:
      temp_dir = args.temp_dir
    else:
      temp_dir = _MakeOutputPath()
    _Print('Using %s' % temp_dir)
    if not args.keep:
      cleanup_fcn.append(lambda: _Clean(temp_dir))
  try:
    if args.command:
      _UnpackArchive(args.archive, temp_dir)
      # Try to resolve the command arguments as a trampoline first.
      if _TryGetTrampoline(temp_dir, args.command):
        _RunTrampoline(temp_dir, args.command, args.command_arg)
      else:
        # Otherwise attempt to run the command as a normal command rooted at
        # the cobalt archive unpacked directory.
        _RunCommand(temp_dir, args.command, args.command_arg)
  finally:
    [f() for f in cleanup_fcn]


if __name__ == '__main__':
  try:
    main()
    sys.exit(0)
  except Exception as err:  # pylint: disable=broad-except
    logging.exception(err)
    sys.exit(1)

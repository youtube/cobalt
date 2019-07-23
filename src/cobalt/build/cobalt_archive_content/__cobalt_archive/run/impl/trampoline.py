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


"""Trampoline resolves a trampoline to a command string."""


import argparse
import json
import os
import subprocess
import sys


################################################################################
#                                  API                                         #
################################################################################


def RunTrampolineThenExit(trampoline, argv=None):
  cmd_str = ResolveTrampoline(trampoline, argv)
  sys.exit(_ShellCmd(cmd_str))


def ResolveTrampoline(trampoline, argv=None):
  r"""Resolves the trampoline list and returns a fully resolve strings.

  The result of this function call is to return the cmd string with
  the platform, config, device_id resolved to values.
    Example input:
      ['python starboard/tools/example/app_launcher_client.py -t cobalt',
       '{platform_arg}', '{config_arg}', '{device_id_arg}']
    Example Output:
      'python starboard/tools/example/app_launcher_client.py -t cobalt ' + \
      '--platform linux --config devel --device_id IP_ADDRESS'

  Args:
    trampoline: a list of commands mixed in with unresolved symobls.
    argv: a list of known resolves symbols to use in the trampoline.

  Returns:
    A string representing the resolved shell command.
  """
  return _ResolveTrampoline(trampoline, argv)


def RunThenExit(cmd_str, cwd=None):
  if cwd == None:
    cwd = _FindCwd()
  sys.exit(_ShellCmd(cmd_str, cwd=cwd))


################################################################################
#                                 IMPL                                         #
################################################################################


_SELF_DIR = os.path.dirname(__file__)
_META_FILE = os.path.normpath(
    os.path.join(_SELF_DIR, '..', '..', 'metadata.json'))
with open(_META_FILE) as fd:
  data = fd.read()
  _META_DATA = json.loads(data)


# Tests can modify these values and alter the output of the trampoline
# resolver.
PLATFORM = _META_DATA['platform']
CONFIG = _META_DATA['config']
IS_TEST = _META_DATA.get('comment', None) == 'TEST'


def _FindCwd():
  """Gets the current working directory.

  This is detected by whether the metadata.json is real (meaning we are in a
  cobalt archive) or in the source directory.

  Returns:
    current working directory for execution.
  """
  if IS_TEST:
    p = os.path.join(_SELF_DIR, '..', '..', '..', '..', '..', '..')
  else:
    p = os.path.join(_SELF_DIR, '..', '..', '..')
  return os.path.normpath(p)


def _ShellCmd(cmd_str, cwd):
  sys.stdout.write('in:      %s\n' % os.path.abspath(cwd))
  sys.stdout.write('Calling: %s\n\n' % cmd_str)
  return subprocess.call(cmd_str, cwd=cwd, shell=True,
                         universal_newlines=True)


def _UnQuote(s):
  if len(s) < 2:
    return s
  elif s[0] == '"' and s[-1] == '"':
    return s[1:-1]
  else:
    return s


def _ResolveTrampoline(trampoline, argv):
  """Implemention, see ResolveTrampoline() above."""
  if argv is None:
    argv = sys.argv[2:]
  known_args, unknown_args = _ParseArgs(argv)
  placeholders = {
      'platform_arg': '--platform %s' % PLATFORM,
      'config_arg': '--config %s' % CONFIG
  }
  placeholders['device_id_arg'] = (
      '' if not known_args.device_id
      else '--device_id %s' % known_args.device_id)
  placeholders['target_params_arg'] = (
      '' if not known_args.target_params else
      '--target_params="%s"' % _UnQuote(known_args.target_params))
  if not known_args.target_name:
    placeholders['target_names_arg'] = ''
  else:
    targets = ['--target_name ' + name for name in known_args.target_name]
    placeholders['target_names_arg'] = ' '.join(targets)
  trampoline = [part.format(**placeholders) for part in trampoline]
  trampoline = [t for t in trampoline if t.strip()]
  return ' '.join(trampoline), unknown_args


def _ParseArgs(argv):
  """Parses arguments."""

  class MyParser(argparse.ArgumentParser):

    def error(self, message):
      sys.stderr.write('error: %s\n' % message)
      self.print_help()

  formatter_class = argparse.RawDescriptionHelpFormatter
  parser = MyParser(formatter_class=formatter_class)
  parser.add_argument('--device_id', type=str,
                      help=('Choices are a device_id (usually an IP) '
                            'passed to the launcher.'),
                      default=None)
  parser.add_argument('--target_params', type=str,
                      help='Key=Value list of args to pass to the client.',
                      default=None)
  parser.add_argument('-t', '--target_name', action='append',
                      help=('Name of executable target. Repeatable for '
                            'multiple targets.'))
  args = parser.parse_known_args(argv)
  return args

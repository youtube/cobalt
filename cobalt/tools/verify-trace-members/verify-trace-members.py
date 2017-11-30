#!/usr/bin/env python3

# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
A wrapper around the native-trace-members binary

This script is responsible for:

* Getting and translating Clang invocations required to build Cobalt into
  verify-trace-members invocations.
  * The Clang invocations are retreived by asking ninja for them.
  * Then, "clang++ -std=c++11 -c my_file.cc -o my_file.o" becomes
    "verify-trace-members my_file.cc -- -std=c++11", for each command.

* Filtering the output of verify-trace-members.
  * We must remove repeated messages (as we could e.g., encounter the same
    header multiple times in different translation units).

  * We must also remove false positives due to the verify-trace-members output
    being overly eager in what it thinks is a TraceMembers usage error.  It is
    set up like this because changing the binary is painful, whereas changing
    the wrapper script is not, so it's easier to just dump as much as possible
    (relevant stuff) in the binary, and then filter here, rather than
    attempt to filter in the binary.
"""

import json
import logging
import os
import re
import subprocess
import sys

# TODO: Probably pretty easy to speed this up by using multiprocessing for the
# clang tool calls.

LINUX_OUT_DIRS = [
    'out/linux-x64x11_debug',
    'out/linux-x64x11_devel',
    'out/linux-x64x11_qa',
    'out/linux-x64x11_gold',
]

# A little bit of environment set up.  We're going to be invoking a clang-like
# tool as if we were clang, so we need to be in the same dir that clang
# normally is.
found_dir = False
for out_dir in LINUX_OUT_DIRS:
  if os.path.isdir(out_dir):
    found_dir = True
    os.chdir(out_dir)
    break

if not found_dir:
  logging.error('At least one of {} must exist.'.format(LINUX_OUT_DIRS))
  sys.exit(1)


def GetRawClangCommands():
  """Ask ninja to output all the commands that would need to be run in order
  to build the target "all"."""
  p = subprocess.Popen(
      ['ninja', '-t', 'commands', 'all'],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  stdout, stderr = p.communicate()
  assert len(stderr) == 0
  assert p.returncode == 0
  lines = stdout.splitlines()
  raw_commands = [line.decode() for line in lines]
  return raw_commands


def IsClangCommand(command):
  """Determine if a command is a call to clang."""
  no_goma = command[1:] if command[0] == 'gomacc' else command
  if no_goma[0].endswith('bin/clang') or no_goma[0].endswith('bin/clang++'):
    return True
  return False


def CommandStringToList(command_string):
  """Parse a raw command string into a list of separate arguments."""
  # We want to split on spaces, but we still have to respect quotes.
  quote_split = re.split(r" ('.*?') ", command_string)
  split = []
  for item in quote_split:
    if item.startswith("'"):
      split.append(item[1:-1])
    else:
      for sub_item in item.split(' '):
        split.append(sub_item)
  return split


def FindSourceFile(command):
  """Extract the sole source file being compiled in a clang command.

  Return None if no such file is found.
  """
  if len(command) < 2:
    return None
  if not ('clang' in command[0] or 'clang' in command[1]):
    return None

  for i, arg in enumerate(command):
    if arg == '-c':
      # Don't even bother trying to handle "-c" being the last arg, just
      # crash.
      assert i + 1 < len(command)
      return command[i + 1]

  return None


def StripRelative(file_arg):
  """ "../../cobalt/dom/document.cc" -> "cobalt/dom/document.cc" """
  return re.sub(r'^(\.\./)+', '', file_arg)


def ConvertClangCommandToVerifyTraceMembers(clang_command):
  """Transform a clang invocation into a verify-trace-members invocation.

  Returns None for non clang invocations.
  """

  source_file = FindSourceFile(clang_command)
  if source_file is None:
    return None

  without_cxx = clang_command
  if without_cxx[0].startswith('goma'):
    without_cxx = without_cxx[1:]
  without_cxx = without_cxx[1:]

  # We want to keep everything except for "-c *" (which we already extracted),
  # and "-o *".
  result = []
  i = 0
  while i < len(without_cxx):
    arg = without_cxx[i]
    if arg == '-c':
      i += 2
      continue
    if arg == '-o':
      i += 2
      continue
    result.append(arg)
    i += 1

  result = [
      # TODO: Need to put binary on google cloud, add hook to pull it from
      # there, and add to gitignore.
      '../../cobalt/tools/verify-trace-members',
      source_file,
      '--'
  ] + result
  # The tool is built against trunk clang, which has some warnings that we
  # don't pass, therefore we must turn off -Wall.
  result = [item for item in result if item != '-Wall']
  return result


doesnt_need_call_base_trace_members = {
    'class cobalt::script::Wrappable',
    'class cobalt::script::Traceable',
}


def main():
  # TODO: Add arguments via argparse

  raw_commands = GetRawClangCommands()
  commands = [CommandStringToList(raw_command) for raw_command in raw_commands]

  suggestions = set()
  for command in commands:
    tool_command = ConvertClangCommandToVerifyTraceMembers(command)
    if tool_command is None:
      continue
    source_file = FindSourceFile(command)
    # TODO: Accept list of files as arguments.
    if not source_file.startswith('../../cobalt/dom/'):
      continue
    p = subprocess.Popen(
        tool_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    if len(stderr.decode()) != 0 or p.returncode != 0:
      logging.error(stderr)
      exit(1)
    for line in stdout.decode().splitlines():
      # Load and then dump to JSON to standardize w.r.t. to whitespace/order, so
      # we can use the stringified object itself as a key into a set.
      suggestions.add(json.dumps(json.loads(line), sort_keys=True))

  suggestions = [json.loads(s) for s in suggestions]

  # Filter out some special cases that we should ignore.
  actual_suggestions = []
  for suggestion in suggestions:
    message_type = suggestion['messageType']

    if 'fieldClass' in suggestion:
      field_class = suggestion['fieldClass']
      # scoped_refptr and scoped_ptr are currently the only way to signal
      # ownership.
      if not ('scoped_refptr' in field_class or 'scoped_ptr' in field_class):
        continue

    if message_type == 'needsTraceMembersDeclaration':
      pass
    elif message_type == 'needsTracerTraceField':
      pass
    elif message_type == 'needsCallBaseTraceMembers':
      base_names = suggestion['baseNames']
      if any(
          name in doesnt_need_call_base_trace_members for name in base_names):
        continue
    else:
      assert False

    actual_suggestions.append(suggestion)

  for suggestion in actual_suggestions:
    message_type = suggestion['messageType']
    if message_type == 'needsTraceMembersDeclaration':
      parent_class_friendly = suggestion['parentClassFriendly']
      field_name = suggestion['fieldName']
      print('{} needs to declare TraceMembers because of field {}'.format(
          parent_class_friendly, field_name))
      print('  void TraceMembers(script::Tracer* tracer) override;')
    elif message_type == 'needsTracerTraceField':
      parent_class_friendly = suggestion['parentClassFriendly']
      field_name = suggestion['fieldName']
      print('{} needs to trace field {}'.format(parent_class_friendly,
                                                field_name))
      print('  tracer->Trace({});'.format(field_name))
    elif message_type == 'needsCallBaseTraceMembers':
      parent_class_friendly = suggestion['parentClassFriendly']
      base_names = suggestion['baseNames']
      print(
          '{} needs to call base class TraceMembers in its TraceMembers'.format(
              parent_class_friendly))
      print('Something like (this is probably over-qualified):')
      for base_name in base_names:
        print('  {}::TraceMembers(tracer);'.format(base_name))
    else:
      assert False

    # TODO: Put this under a verbose output command line argument.
    print(json.dumps(
        suggestion, sort_keys=True, indent=4, separators=(',', ': ')))
    print('')

  return 0


if __name__ == '__main__':
  sys.exit(main())

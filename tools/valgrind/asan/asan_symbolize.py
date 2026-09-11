#!/usr/bin/env python3

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Symbolizes sanitizer reports and test summary JSON files.

Delegates core symbolization to starboard.tools.symbolize for high performance
and multi-binary session tracking while preserving backward compatibility for
Chromium test launcher workflows.
"""

import argparse
import base64
import json
import os
import platform
import re
import subprocess
import sys

_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
if _SRC_DIR not in sys.path:
  sys.path.insert(0, _SRC_DIR)

from starboard.tools.symbolize.json_processor import process_test_run
from starboard.tools.symbolize.json_processor import process_test_summary_json
from starboard.tools.symbolize.runner import SymbolizerRunner
from starboard.tools.symbolize.symbolize import symbolize_stream
from starboard.tools.symbolize.symbolize import symbolize_string
from tools.valgrind.asan.third_party import asan_symbolize


class LineBuffered(object):
  """Disable buffering on a file object."""

  def __init__(self, stream):
    self.stream = stream

  def write(self, data):
    self.stream.write(data)
    if '\n' in data:
      self.stream.flush()

  def __getattr__(self, attr):
    return getattr(self.stream, attr)


def disable_buffering():
  """Makes this process and child processes stdout unbuffered."""
  if not os.environ.get('PYTHONUNBUFFERED'):
    sys.stdout = LineBuffered(sys.stdout)
    os.environ['PYTHONUNBUFFERED'] = 'x'


def set_symbolizer_path():
  """Set the path to the llvm-symbolize binary in the Chromium source tree."""
  if not os.environ.get('LLVM_SYMBOLIZER_PATH'):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    src_root = os.path.join(script_dir, '..', '..', '..')
    symbolizer_path = os.path.join(src_root, 'third_party', 'llvm-build',
                                   'Release+Asserts', 'bin', 'llvm-symbolizer')
    assert os.path.isfile(symbolizer_path)
    os.environ['LLVM_SYMBOLIZER_PATH'] = os.path.abspath(symbolizer_path)


def is_hash_name(name):
  match = re.match('[0-9a-f]+$', name)
  return bool(match)


def split_path(path):
  ret = []
  while True:
    head, tail = os.path.split(path)
    if head == path:
      return [head] + ret
    ret, path = [tail] + ret, head


def chrome_product_dir_path(exe_path):
  if exe_path is None:
    return None
  path_parts = split_path(exe_path)
  if len(path_parts) == 1:
    path_parts = ['.'] + path_parts
  for index, part in enumerate(path_parts):
    if part.endswith('.app'):
      return os.path.join(*path_parts[:index])
  return os.path.join(*path_parts[:-1])


inode_path_cache = {}


def find_inode_at_path(inode, path):
  if inode in inode_path_cache:
    return inode_path_cache[inode]
  cmd = ['find', path, '-inum', str(inode)]
  find_line = subprocess.check_output(cmd).rstrip()
  lines = find_line.split('\n')
  ret = None
  if lines:
    ret = lines[0]
  inode_path_cache[inode] = ret
  return ret


def chrome_dsym_hints(binary):
  path_parts = split_path(binary)
  app_positions = []
  framework_positions = []
  for index, part in enumerate(path_parts):
    if part.endswith('.app'):
      app_positions.append(index)
    elif part.endswith('.framework'):
      framework_positions.append(index)
  bundle_positions = app_positions + framework_positions
  bundle_positions.sort()
  assert len(bundle_positions) <= 2, \
      "The path contains more than two nested bundles: %s" % binary
  if len(bundle_positions) == 0:
    return []
  assert (not (len(app_positions) == 1 and
               len(framework_positions) == 1 and
               app_positions[0] > framework_positions[0])), \
      "The path contains an app bundle inside a framework: %s" % binary
  outermost_bundle = bundle_positions[0]
  product_dir = path_parts[:outermost_bundle]
  innermost_bundle = bundle_positions[-1]
  dsym_path = product_dir + [path_parts[innermost_bundle]]
  result = '%s.dSYM' % os.path.join(*dsym_path)
  return [result]


class JSONTestRunSymbolizer(object):
  """Symbolizer for test run snippets within test_summary.json."""

  def __init__(self, symbolization_loop):
    self.symbolization_loop = symbolization_loop

  def symbolize_snippet(self, snippet):
    symbolized_lines = []
    for line in snippet.split('\n'):
      symbolized_lines += self.symbolization_loop.process_line(line)
    return '\n'.join(symbolized_lines)

  def symbolize(self, test_run):
    original_snippet = base64.b64decode(
        test_run['output_snippet_base64']).decode('utf-8', 'replace')

    # replace non-ascii character with '?'.
    original_snippet = ''.join(i if i <= u'~' else u'?'
                               for i in original_snippet)

    symbolized_snippet = self.symbolize_snippet(original_snippet)
    if symbolized_snippet == original_snippet:
      return

    test_run['original_output_snippet'] = test_run['output_snippet']
    test_run['original_output_snippet_base64'] = (
        test_run['output_snippet_base64'])

    test_run['output_snippet'] = symbolized_snippet
    test_run['output_snippet_base64'] = (base64.b64encode(
        symbolized_snippet.encode('utf-8', 'replace')).decode())
    test_run['snippet_processed_by'] = 'asan_symbolize.py'


def symbolize_snippets_in_json(filename, symbolization_loop):
  with open(filename, 'r', encoding='utf-8') as f:
    json_data = json.load(f)

  test_run_symbolizer = JSONTestRunSymbolizer(symbolization_loop)
  for iteration_data in json_data.get('per_iteration_data', []):
    for _, test_runs in iteration_data.items():
      for test_run in test_runs:
        test_run_symbolizer.symbolize(test_run)

  with open(filename, 'w', encoding='utf-8') as f:
    json.dump(json_data, f, indent=3, sort_keys=True)


class macOSBinaryNameFilterPlugin(asan_symbolize.AsanSymbolizerPlugIn):
  """Filters binary paths on macOS swarming bots."""

  def __init__(self):
    self.product_dir_path = ''

  def filter_binary_path(self, binary_path):
    basename = os.path.basename(binary_path)
    if is_hash_name(basename) and self.product_dir_path:
      inode = os.stat(binary_path).st_ino
      new_binary_path = find_inode_at_path(inode, self.product_dir_path)
      if new_binary_path:
        return new_binary_path
    return binary_path


class CheckUTF8:
  """Wraps stream and emits warnings if stream contains invalid UTF-8."""

  def __init__(self, stream):
    self._stream = stream

  def __iter__(self):
    return self

  def __next__(self):
    line = self._stream.buffer.readline()
    if not line:
      raise StopIteration

    try:
      return line.decode()
    except UnicodeDecodeError:
      print('WARNING: asan_symbolize.py failed to decode %s (base64 encoded)' %
            base64.b64encode(line).decode())
      return ''


def main():
  parser = argparse.ArgumentParser(description='Symbolize sanitizer reports.')
  parser.add_argument(
      '--test-summary-json-file',
      help='Path to a JSON file produced by the test launcher. The script will '
      'ignore standard input and instead symbolize the output snippets '
      'inside the JSON file. The result will be written back to the JSON '
      'file.')
  parser.add_argument(
      'strip_path_prefix',
      nargs='*',
      help='When printing source file names, the longest prefix ending in one '
      'of these substrings will be stripped. E.g.: "Release/../../".')
  parser.add_argument(
      '--executable-path',
      help='Path to program executable. Used on OSX swarming bots to locate '
      'dSYM bundles for associated frameworks and bundles.')
  parser.add_argument('--sysroot', help='Root directory for symbol files')
  # Cobalt customizations
  parser.add_argument(
      '--extra-binary',
      default=None,
      help='Path to a dynamically loaded binary for which to symbolize '
      'stack traces.')
  # End Cobalt customizations
  args = parser.parse_args()

  # Cobalt customizations
  if args.extra_binary and not os.path.exists(args.extra_binary):
    raise ValueError(f'Extra binary not found at: {args.extra_binary}\n')
  # End Cobalt customizations

  disable_buffering()
  set_symbolizer_path()

  strip_prefixes = list(args.strip_path_prefix or [])
  strip_prefixes.append('Release/../../')

  with SymbolizerRunner(default_library=args.extra_binary) as runner:
    if args.test_summary_json_file:

      def symbolize_fn(lines):
        return symbolize_string(''.join(lines),
                                library=args.extra_binary,
                                runner=runner,
                                strip_prefixes=strip_prefixes)

      process_test_summary_json(args.test_summary_json_file,
                                symbolize_fn,
                                processor_tag='asan_symbolize.py')
    else:
      in_stream = CheckUTF8(sys.stdin)
      symbolize_stream(in_stream=in_stream,
                       out_stream=sys.stdout,
                       library=args.extra_binary,
                       runner=runner,
                       strip_prefixes=strip_prefixes)


if __name__ == '__main__':
  main()

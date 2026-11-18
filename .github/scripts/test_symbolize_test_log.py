#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Tests for symbolize_test_log."""

import os
import sys
import tempfile
import types
import unittest
from unittest import mock

import symbolize_test_log

_LOG = ('[INFO:elf_loader_impl.cc(59)] Loading /work/out/libnplb.so\n'
        '[ RUN      ] Foo.Bar\n'
        'Caught signal: SIGSEGV (11)\n'
        '\t<unknown> [0x174dd22]\n'
        '\tSbEventHandle [0x7f6343b0a88a]\n')


class SymbolizeTestLogTest(unittest.TestCase):

  def setUp(self):
    self._tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.root = self._tmp.name
    self.source_dir = os.path.join(self.root, 'src')
    self.out_dir = os.path.join(self.source_dir, 'out')
    self.log_path = os.path.join(self.root, 'nplb_loader_log.txt')
    self.symbolizer = os.path.join(self.source_dir, 'third_party', 'llvm-build',
                                   'Release+Asserts', 'bin', 'llvm-symbolizer')
    self.unstripped = os.path.join(self.out_dir, 'lib.unstripped', 'libnplb.so')

  def tearDown(self):
    self._tmp.cleanup()

  def _write(self, path, content='', executable=False):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
      f.write(content)
    if executable:
      os.chmod(path, 0o755)

  def _read_log(self):
    with open(self.log_path, 'r', encoding='utf-8') as f:
      return f.read()

  def _fake_symbolize_modules(self, calls):

    def symbolize_stream(in_stream, out_stream, library):
      calls.append(library)
      for line in in_stream:
        out_stream.write(
            line.replace('<unknown> [0x174dd22]', '0x174dd22 [Foo::Bar()]'))

    module = types.ModuleType('starboard.tools.symbolize.symbolize')
    module.symbolize_stream = symbolize_stream
    package = types.ModuleType('starboard.tools.symbolize')
    package.symbolize = module
    return {
        'starboard': types.ModuleType('starboard'),
        'starboard.tools': types.ModuleType('starboard.tools'),
        'starboard.tools.symbolize': package,
        'starboard.tools.symbolize.symbolize': module,
    }

  def test_symbolizes_unknown_frames(self):
    self._write(self.log_path, _LOG)
    self._write(self.unstripped)
    self._write(self.symbolizer, executable=True)
    calls = []

    with mock.patch.dict(sys.modules, self._fake_symbolize_modules(calls)), \
        mock.patch.dict(os.environ):
      self.assertTrue(
          symbolize_test_log.symbolize_log(self.log_path, self.out_dir,
                                           self.source_dir))
      self.assertEqual(self.symbolizer, os.environ['LLVM_SYMBOLIZER_PATH'])

    self.assertEqual([self.unstripped], calls)
    log = self._read_log()
    self.assertIn('\t0x174dd22 [Foo::Bar()]\n', log)
    self.assertNotIn('<unknown>', log)
    self.assertIn('\tSbEventHandle [0x7f6343b0a88a]\n', log)

  def test_leaves_log_without_unknown_frames_untouched(self):
    log = _LOG.replace('<unknown> [0x174dd22]', 'Foo::Bar() [0x7f00001234]')
    self._write(self.log_path, log)
    self._write(self.unstripped)
    self._write(self.symbolizer, executable=True)

    self.assertFalse(
        symbolize_test_log.symbolize_log(self.log_path, self.out_dir,
                                         self.source_dir))
    self.assertEqual(log, self._read_log())

  def test_leaves_log_untouched_without_unstripped_library(self):
    self._write(self.log_path, _LOG)
    self._write(self.symbolizer, executable=True)

    self.assertFalse(
        symbolize_test_log.symbolize_log(self.log_path, self.out_dir,
                                         self.source_dir))
    self.assertEqual(_LOG, self._read_log())

  def test_leaves_log_untouched_without_symbolizer(self):
    self._write(self.log_path, _LOG)
    self._write(self.unstripped)

    self.assertFalse(
        symbolize_test_log.symbolize_log(self.log_path, self.out_dir,
                                         self.source_dir))
    self.assertEqual(_LOG, self._read_log())

  def test_leaves_log_untouched_without_loaded_library(self):
    log = _LOG.splitlines(keepends=True)[1:]
    self._write(self.log_path, ''.join(log))
    self._write(self.unstripped)
    self._write(self.symbolizer, executable=True)

    self.assertFalse(
        symbolize_test_log.symbolize_log(self.log_path, self.out_dir,
                                         self.source_dir))
    self.assertEqual(''.join(log), self._read_log())


if __name__ == '__main__':
  unittest.main()

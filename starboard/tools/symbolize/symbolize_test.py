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
"""Unit, integration, and benchmark tests for the symbolize package."""

# pylint: disable=protected-access,wrong-import-position,consider-using-with

import base64
import io
import os
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
if _SRC_DIR not in sys.path:
  sys.path.insert(0, _SRC_DIR)

from starboard.tools import paths
from starboard.tools.symbolize import detector
from starboard.tools.symbolize import formats
from starboard.tools.symbolize import json_processor
from starboard.tools.symbolize import runner
from starboard.tools.symbolize import symbolize

_CLANG = os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'llvm-build',
                      'Release+Asserts', 'bin', 'clang')
_LLVM_NM = os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'llvm-build',
                        'Release+Asserts', 'bin', 'llvm-nm')
_SYMBOLIZER = os.path.join(paths.REPOSITORY_ROOT, 'third_party', 'llvm-build',
                           'Release+Asserts', 'bin', 'llvm-symbolizer')
_TESTDATA_DIR = os.path.join(os.path.dirname(__file__), 'testdata')


class _FakeSymbolizerRunner:
  """Mock runner to supply deterministic responses without spawning LLVM."""

  def __init__(self, responses=None, default_response=None):
    self._responses = responses or {}
    self._default_response = default_response
    self.calls = []
    self.closed = False

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self.close()

  def close(self):
    self.closed = True

  def symbolize(self, offset, binary=None):  # pylint: disable=unused-argument
    self.calls.append(offset)
    if offset in self._responses:
      return self._responses[offset]
    return self._default_response


class SymbolizeUnitTests(unittest.TestCase):
  """Tests parsing, regexes, offset arithmetic, and streams in isolation."""

  def test_rdk_syslog_prefix_preservation(self):
    line = ('[2026-09-01 01:28:54:229 PDT] Sep 01 08:28:53 AmlogicFirebolt '
            'YouTube[2001]: \t<unknown> [0x10e2de2]\n')
    fake_runner = _FakeSymbolizerRunner({
        str(0x10e2de2): [
            'cobalt::CobaltBrowserMainParts::PreCreateThreads()',
            'cobalt_browser_main_parts.cc:316'
        ]
    })
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    expected = ('[2026-09-01 01:28:54:229 PDT] Sep 01 08:28:53 AmlogicFirebolt '
                'YouTube[2001]: \t0x10e2de2 '
                '[cobalt::CobaltBrowserMainParts::PreCreateThreads()]\n')
    self.assertEqual(out_stream.getvalue(), expected)

  def test_android_logcat_prefix_and_tombstone(self):
    logcat_line = (
        '09-08 17:27:44.416  8169  8169 E chromium: #00 pc 0x03bef795 '
        '/data/app/dev.cobalt.coat/lib/arm/libchrobalt.so\n')
    tombstone_line = (
        '09-08 17:27:46.387  8283  8283 F DEBUG   :       #00 pc 01115620  '
        '/data/app/dev.cobalt.coat/lib/arm/libchrobalt.so (BuildId: 3abe1846)\n'
    )
    fake_runner = _FakeSymbolizerRunner({
        str(0x3bef795): [
            'base::debug::StackTrace::StackTrace()', 'stack_trace.cc:255'
        ],
        str(0x1115620): [
            'cobalt::CobaltBrowserMainParts::PreCreateThreads()',
            'cobalt_browser_main_parts.cc:319'
        ]
    })
    in_stream = io.StringIO(logcat_line + tombstone_line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    output = out_stream.getvalue().splitlines(keepends=True)
    self.assertEqual(len(output), 2)
    self.assertIn(
        '09-08 17:27:44.416  8169  8169 E chromium: #00 pc 0x3bef795 in '
        'base::debug::StackTrace::StackTrace() stack_trace.cc:255 '
        '(/data/app/dev.cobalt.coat/lib/arm/libchrobalt.so)\n', output[0])
    self.assertIn(
        '09-08 17:27:46.387  8283  8283 F DEBUG   :       #00 pc 0x1115620 in '
        'cobalt::CobaltBrowserMainParts::PreCreateThreads() '
        'cobalt_browser_main_parts.cc:319 '
        '(/data/app/dev.cobalt.coat/lib/arm/libchrobalt.so '
        '(BuildId: 3abe1846))\n', output[1])

  def test_asan_format(self):
    line = '    #1 0x7fdc59bbaa6b  (<unknown module>)\n'
    fake_runner = _FakeSymbolizerRunner(
        {str(0x7fdc59bbaa6b): ['asan_detected_leak()', 'asan.cc:42']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(
        out_stream.getvalue(),
        '    #1 0x7fdc59bbaa6b in asan_detected_leak() asan.cc:42\n')

  def test_gdb_format(self):
    line = '    #1  0x742a51b6 in ?? () from /lib/libcobalt.so\n'
    fake_runner = _FakeSymbolizerRunner(
        {str(0x742a51b6): ['gdb_resolved_func()', 'gdb.cc:10']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(),
                     '    #1 0x742a51b6 in gdb_resolved_func() gdb.cc:10\n')

  def test_raw_format(self):
    line = '0x7efcdf1fd52b\n'
    fake_runner = _FakeSymbolizerRunner(
        {str(0x7efcdf1fd52b): ['raw_symbol()', 'raw.cc:99']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(),
                     '0x7efcdf1fd52b raw_symbol() in raw.cc:99\n')

  def test_unresolved_question_marks_left_unmodified(self):
    line = '        <unknown> [0x12345]\n'
    fake_runner = _FakeSymbolizerRunner({str(0x12345): ['??', '??:0']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(), line)

  def test_single_line_response_no_index_error(self):
    line = '        <unknown> [0x12345]\n'
    fake_runner = _FakeSymbolizerRunner({str(0x12345): ['only_func_name']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(),
                     '        0x12345 [only_func_name]\n')

  def test_offset_arithmetic_relative_vs_absolute(self):
    base_addr = '0x7f0000000000'
    rel_line = '        <unknown> [0x1000]\n'
    abs_line = '        <unknown> [0x7f0000002000]\n'
    fake_runner = _FakeSymbolizerRunner({
        str(0x1000): ['func_rel()'],
        str(0x2000): ['func_abs()']
    })
    in_stream = io.StringIO(rel_line + abs_line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address=base_addr)
    self.assertIn(str(0x1000), fake_runner.calls)
    self.assertIn(str(0x2000), fake_runner.calls)

  def test_integer_base_address_handling(self):
    line = '        <unknown> [0x1000]\n'
    fake_runner = _FakeSymbolizerRunner({str(0x1000): ['func_rel()']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    # Ensure integer 0 and 0x1000 don't raise TypeError
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address=0)
    self.assertIn('func_rel()', out_stream.getvalue())

  def test_symbolizer_runner_dead_pipe_recovery(self):
    test_runner = runner.SymbolizerRunner(
        library='/nonexistent/lib.so', symbolizer_path='/bin/false')
    res = test_runner.symbolize('4096')
    self.assertIsNone(res)
    self.assertIsNone(test_runner._proc)

  def test_symbolizer_runner_caching(self):
    test_runner = runner.SymbolizerRunner(library='/nonexistent/lib.so')
    test_runner._cache[('/nonexistent/lib.so', 1234)] = [('cached_func()',
                                                          'file.cc:1')]
    self.assertEqual(
        test_runner.symbolize('1234'), [('cached_func()', 'file.cc:1')])
    self.assertIsNone(test_runner._proc)

  def test_golden_collected_files_process_cleanly(self):
    for filename in [
        'rdk_syslog_stack.log', 'rdk_device_live.log',
        'evergreen_linux_stack.log', 'android_chromecast_stack.log'
    ]:
      log_path = os.path.join(_TESTDATA_DIR, filename)
      if os.path.exists(log_path):
        fake_runner = _FakeSymbolizerRunner(
            default_response=['mock_resolved_symbol()', 'file.cc:1'])
        out_stream = io.StringIO()
        with open(log_path, 'r', encoding='utf-8') as f:
          symbolize._Symbolize(
              in_stream=f,
              out_stream=out_stream,
              runner=fake_runner,
              base_address='0')
        self.assertGreater(len(out_stream.getvalue()), 0)
        self.assertGreater(len(fake_runner.calls), 0)
        self.assertIn('mock_resolved_symbol', out_stream.getvalue())

  def test_symbolize_raises_on_missing_file(self):
    with self.assertRaises(ValueError) as ctx:
      symbolize._Symbolize(
          filename='/nonexistent/trace/file.log',
          runner=_FakeSymbolizerRunner())
    self.assertIn('File not found', str(ctx.exception))

  def test_symbolize_raises_on_missing_library(self):
    with self.assertRaises(ValueError) as ctx:
      symbolize._Symbolize(
          in_stream=io.StringIO('test'), library='/nonexistent/lib.so')
    self.assertIn('Library not found', str(ctx.exception))

  def test_symbolize_reads_from_file_path(self):
    with tempfile.NamedTemporaryFile(mode='w+', encoding='utf-8') as f:
      f.write('        <unknown> [0x1000]\n')
      f.flush()
      fake_runner = _FakeSymbolizerRunner(
          {str(0x1000): ['func_from_file()', 'file.cc:10']})
      out_stream = io.StringIO()
      symbolize._Symbolize(
          filename=f.name,
          out_stream=out_stream,
          runner=fake_runner,
          base_address='0')
      self.assertIn('func_from_file()', out_stream.getvalue())

  def test_main_cli_argument_parsing(self):
    with tempfile.NamedTemporaryFile(mode='w+', encoding='utf-8') as f_trace, \
         tempfile.NamedTemporaryFile(mode='w+', encoding='utf-8') as f_lib:
      test_args = [
          'symbolize.py', '-f', f_trace.name, '-l', f_lib.name, '0x1000'
      ]
      with mock.patch('sys.argv', test_args), \
           mock.patch.object(symbolize, '_Symbolize') as mock_symbolize, \
           mock.patch('os.path.exists', return_value=True):
        symbolize.main()
        mock_symbolize.assert_called_once_with(f_trace.name, f_lib.name,
                                               '0x1000')

  def test_main_cli_raises_when_llvm_symbolizer_missing(self):
    test_args = ['symbolize.py', '-f', '/tmp/trace.log', '-l', '/tmp/lib.so']
    with mock.patch('sys.argv', test_args), \
         mock.patch('os.path.exists', return_value=False):
      with self.assertRaises(ValueError) as ctx:
        symbolize.main()
      self.assertIn('Please update', str(ctx.exception))

  def test_symbolizer_runner_negative_offset(self):
    test_runner = runner.SymbolizerRunner(library='/nonexistent/lib.so')
    self.assertIsNone(test_runner.symbolize('-1'))
    self.assertIsNone(test_runner.symbolize('-4096'))

  def test_symbolizer_runner_context_manager(self):
    test_runner = runner.SymbolizerRunner(library='/nonexistent/lib.so')
    mock_proc = mock.MagicMock()
    with test_runner:
      test_runner._proc = mock_proc
    mock_proc.stdin.close.assert_called_once()
    mock_proc.stdout.close.assert_called_once()
    mock_proc.wait.assert_called_once()
    self.assertIsNone(test_runner._proc)

  def test_symbolizer_runner_exception_during_write_closes_proc(self):
    test_runner = runner.SymbolizerRunner(library='/nonexistent/lib.so')
    mock_proc = mock.MagicMock()
    mock_proc.stdin.write.side_effect = BrokenPipeError('Broken pipe')
    test_runner._proc = mock_proc
    result = test_runner.symbolize('100')
    self.assertIsNone(result)
    self.assertIsNone(test_runner._proc)
    mock_proc.wait.assert_called_once()

  def test_symbolizer_runner_close_handles_exceptions_gracefully(self):
    test_runner = runner.SymbolizerRunner(library='/nonexistent/lib.so')
    mock_proc = mock.MagicMock()
    mock_proc.stdin.close.side_effect = OSError('Stream error')
    test_runner._proc = mock_proc
    test_runner.close()
    self.assertIsNone(test_runner._proc)
    mock_proc.stdout.close.assert_called_once()
    mock_proc.wait.assert_called_once()

  def test_symbolizer_runner_popen_failure_returns_none(self):
    test_runner = runner.SymbolizerRunner(
        library='/nonexistent/lib.so',
        symbolizer_path='/nonexistent/path/to/llvm-symbolizer')
    with mock.patch(
        'subprocess.Popen', side_effect=FileNotFoundError('No such file')):
      result = test_runner.symbolize('100')
      self.assertIsNone(result)
      self.assertIsNone(test_runner._proc)

  def test_unresolved_all_formats_left_unmodified(self):
    lines = ('    #1 0x7fdc59bbaa6b  (<unknown module>)\n'
             '#00 pc 0x03bef795 /lib/libcobalt.so\n'
             '    #1  0x742a51b6 in ?? () from /lib/libcobalt.so\n')
    fake_runner = _FakeSymbolizerRunner(default_response=['??', '??:0'])
    in_stream = io.StringIO(lines)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(), lines)

  def test_raw_format_includes_unresolved_symbols(self):
    line = '0x7efcdf1fd52b\n'
    fake_runner = _FakeSymbolizerRunner({str(0x7efcdf1fd52b): ['??', '??:0']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(), '0x7efcdf1fd52b ??\n')

  def test_single_line_response_all_formats(self):
    input_text = ('    #1 0x7fdc59bbaa6b  (<unknown module>)\n'
                  '#00 pc 0x03bef795 /lib/libcobalt.so\n'
                  '    #1  0x742a51b6 in ?? () from /lib/libcobalt.so\n'
                  '0x7efcdf1fd52b\n')
    fake_runner = _FakeSymbolizerRunner({
        str(0x7fdc59bbaa6b): ['asan_func'],
        str(0x3bef795): ['android_func'],
        str(0x742a51b6): ['gdb_func'],
        str(0x7efcdf1fd52b): ['raw_func']
    })
    in_stream = io.StringIO(input_text)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    output = out_stream.getvalue().splitlines()
    self.assertIn('asan_func', output[0])
    self.assertIn('android_func', output[1])
    self.assertIn('gdb_func', output[2])
    self.assertIn('raw_func', output[3])

  def test_cobalt_format_with_existing_symbol_name(self):
    line = '        SbEventHandle [0x9c17b5]\n'
    fake_runner = _FakeSymbolizerRunner(
        {str(0x9c17b5): ['NewSymbolName()', 'event.cc:50']})
    in_stream = io.StringIO(line)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(),
                     '        0x9c17b5 [NewSymbolName()]\n')

  def test_non_matching_lines_passed_through_untouched(self):
    lines = ('\n'
             '[0909/002302.313880:INFO:thread.cc(154)] Thread started\n'
             'Check failed: g_sb_event_func.\n'
             'Some arbitrary console output\n')
    fake_runner = _FakeSymbolizerRunner()
    in_stream = io.StringIO(lines)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    self.assertEqual(out_stream.getvalue(), lines)
    self.assertEqual(len(fake_runner.calls), 0)

  def test_hex_casing_variations(self):
    lines = ('    #1 0x7FDC59BBAA6B  (<unknown module>)\n'
             '#00 pc 0x03BEF795 /lib/libcobalt.so\n'
             '        <unknown> [0x10E2DE2]\n'
             '0x7EFCDF1FD52B\n')
    fake_runner = _FakeSymbolizerRunner({
        str(0x7fdc59bbaa6b): ['asan_fn()'],
        str(0x3bef795): ['android_fn()'],
        str(0x10e2de2): ['cobalt_fn()'],
        str(0x7efcdf1fd52b): ['raw_fn()']
    })
    in_stream = io.StringIO(lines)
    out_stream = io.StringIO()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    output = out_stream.getvalue()
    self.assertIn('asan_fn()', output)
    self.assertIn('android_fn()', output)
    self.assertIn('cobalt_fn()', output)
    self.assertIn('raw_fn()', output)


class RunnerUnitTests(unittest.TestCase):
  """Unit tests for runner.py."""

  def test_inlined_frames_parsing(self):
    test_runner = runner.SymbolizerRunner(library='/mock/lib.so')
    mock_proc = mock.MagicMock()
    # Return two pairs of lines representing inlined frames terminated by
    # an empty line.
    mock_proc.stdout.readline.side_effect = [
        'inlined_child()\n',
        'child.h:12:4\n',
        'parent_func()\n',
        'parent.cc:88:2\n',
        '\n',
    ]
    test_runner._proc = mock_proc

    res = test_runner.symbolize('0x1000')
    self.assertEqual(len(res), 2)
    self.assertEqual(res[0], ('inlined_child()', 'child.h:12:4'))
    self.assertEqual(res[1], ('parent_func()', 'parent.cc:88:2'))

  def test_lru_cache_eviction(self):
    test_runner = runner.SymbolizerRunner(
        library='/mock/lib.so', max_cache_size=2)
    test_runner._cache[('/mock/lib.so', 1)] = [('f1', 'l1')]
    test_runner._cache[('/mock/lib.so', 2)] = [('f2', 'l2')]
    # Query key 1 to move it to end
    test_runner.symbolize('1')
    mock_proc = mock.MagicMock()
    mock_proc.stdout.readline.side_effect = ['f3\n', 'l3\n', '\n']
    test_runner._proc = mock_proc
    test_runner.symbolize('3')

    self.assertNotIn(('/mock/lib.so', 2), test_runner._cache)
    self.assertIn(('/mock/lib.so', 1), test_runner._cache)
    self.assertIn(('/mock/lib.so', 3), test_runner._cache)


class FormatsUnitTests(unittest.TestCase):
  """Unit tests for formats.py."""

  def test_asan_inlined_frame_expansion(self):
    handler = formats.AsanMode1FormatHandler()
    match = handler.match('    #0 0x7f48e7a45000  (<unknown module>)\n')
    self.assertIsNotNone(match)

    results = [
        ('inlined_leaf()', '/out/Release/../../leaf.h:10'),
        ('outer_caller()', '/out/Release/../../caller.cc:50'),
    ]
    formatted = handler.format(match, results, resolved_offset=0x1000)
    self.assertEqual(len(formatted), 2)
    self.assertEqual(formatted[0],
                     '    #0 0x1000 in inlined_leaf() leaf.h:10\n')
    self.assertEqual(formatted[1],
                     '    #1 0x1000 in outer_caller() caller.cc:50\n')

  def test_strip_path_prefix(self):
    path = '/usr/local/cobalt/src/out/Release/../../cobalt/dom/node.cc:154'
    self.assertEqual(formats.strip_path_prefix(path), 'cobalt/dom/node.cc:154')


class DetectorUnitTests(unittest.TestCase):
  """Unit tests for detector.py."""

  def test_multi_session_tracking(self):
    tracker = detector.StreamingSessionTracker()
    self.assertIsNone(tracker.current_base_address)
    self.assertEqual(tracker.session_id, 0)

    tracker.check_line('Load start=0x7f1000000000')
    self.assertEqual(tracker.current_base_address, 0x7f1000000000)
    self.assertEqual(tracker.session_id, 1)

    tracker.check_line('Load start=0x7f2000000000')
    self.assertEqual(tracker.current_base_address, 0x7f2000000000)
    self.assertEqual(tracker.session_id, 2)

  def test_three_tier_resolution_modes(self):
    tracker = detector.StreamingSessionTracker(
        default_base_address=0x7f1000000000)

    # Tier 1: 64-bit ASLR address
    offset, mode = tracker.resolve_offset(0x7f1000005000)
    self.assertEqual(offset, 0x5000)
    self.assertEqual(mode, 'base_subtracted')

    # Tier 1: Relative offset (< 100MB)
    offset, mode = tracker.resolve_offset(0x2000)
    self.assertEqual(offset, 0x2000)
    self.assertEqual(mode, 'base_relative')

    # Tier 3: Missing base address
    tracker_no_base = detector.StreamingSessionTracker(
        default_base_address=None)
    offset, mode = tracker_no_base.resolve_offset(0x7f1000005000)
    self.assertIsNone(offset)
    self.assertEqual(mode, 'missing_base')


class JsonProcessorUnitTests(unittest.TestCase):
  """Unit tests for json_processor.py."""

  def test_prefilter_skips_non_trace_snippets(self):
    test_run = {
        'output_snippet':
            'All unit tests passed with 0 errors.',
        'output_snippet_base64':
            base64.b64encode(b'All unit tests passed with 0 errors.').decode()
    }
    mock_sym_fn = mock.MagicMock()
    modified = json_processor.process_test_run(test_run, mock_sym_fn)
    self.assertFalse(modified)
    mock_sym_fn.assert_not_called()

  def test_prefilter_processes_trace_snippets(self):
    test_run = {
        'output_snippet':
            'CRASH LOG: #0 0x1000 in test',
        'output_snippet_base64':
            base64.b64encode(b'CRASH LOG: #0 0x1000 in test').decode()
    }
    mock_sym_fn = mock.MagicMock(
        return_value='CRASH LOG: #0 0x1000 in Symbolized()')
    modified = json_processor.process_test_run(test_run, mock_sym_fn)
    self.assertTrue(modified)
    self.assertEqual(test_run['output_snippet'],
                     'CRASH LOG: #0 0x1000 in Symbolized()')


class MultiBinaryIntegrationTests(unittest.TestCase):
  """Compiles two synthetic C shared libraries to test multi-binary dispatch."""

  temp_dir = None
  alpha_so = None
  beta_so = None
  alpha_offset = None
  beta_offset = None

  @classmethod
  def setUpClass(cls):
    if not (os.path.exists(_CLANG) and os.path.exists(_LLVM_NM) and
            os.path.exists(_SYMBOLIZER)):
      return

    cls.temp_dir = tempfile.TemporaryDirectory()
    alpha_c = os.path.join(cls.temp_dir.name, 'alpha.c')
    beta_c = os.path.join(cls.temp_dir.name, 'beta.c')
    cls.alpha_so = os.path.join(cls.temp_dir.name, 'libalpha.so')
    cls.beta_so = os.path.join(cls.temp_dir.name, 'libbeta.so')

    with open(alpha_c, 'w', encoding='utf-8') as f:
      f.write('int alpha_function(int x) { return x + 10; }\n')
    with open(beta_c, 'w', encoding='utf-8') as f:
      f.write('int beta_function(int y) { return y * 20; }\n')

    subprocess.check_call(
        [_CLANG, '-shared', '-fPIC', '-g', '-O0', alpha_c, '-o', cls.alpha_so])
    subprocess.check_call(
        [_CLANG, '-shared', '-fPIC', '-g', '-O0', beta_c, '-o', cls.beta_so])

    nm_alpha = subprocess.check_output([_LLVM_NM, '-n', cls.alpha_so],
                                       text=True)
    for line in nm_alpha.splitlines():
      parts = line.split()
      if len(parts) >= 3 and parts[2] == 'alpha_function':
        cls.alpha_offset = hex(int(parts[0], 16))

    nm_beta = subprocess.check_output([_LLVM_NM, '-n', cls.beta_so], text=True)
    for line in nm_beta.splitlines():
      parts = line.split()
      if len(parts) >= 3 and parts[2] == 'beta_function':
        cls.beta_offset = hex(int(parts[0], 16))

  @classmethod
  def tearDownClass(cls):
    if cls.temp_dir:
      cls.temp_dir.cleanup()

  def setUp(self):
    if not (os.path.exists(_CLANG) and os.path.exists(_LLVM_NM) and
            os.path.exists(_SYMBOLIZER)):
      self.skipTest('LLVM toolchain binaries not found.')

  def test_persistent_runner_switches_binaries(self):
    with runner.SymbolizerRunner() as sym_runner:
      res_alpha = sym_runner.symbolize(self.alpha_offset, binary=self.alpha_so)
      res_beta = sym_runner.symbolize(self.beta_offset, binary=self.beta_so)

      self.assertIsNotNone(res_alpha)
      self.assertIsNotNone(res_beta)
      self.assertIn('alpha_function', res_alpha[0][0])
      self.assertIn('beta_function', res_beta[0][0])


class PerformanceBenchmarkTest(unittest.TestCase):
  """Performance test asserting 1,000+ frame symbolization under 2.0s."""

  def test_thousand_frame_crash_benchmark(self):
    fake_runner = _FakeSymbolizerRunner(
        default_response=['benchmarked_symbol()', 'benchmark.cc:100'])
    lines = [
        f'    #{i} 0x{1000 + i:x}  (<unknown module>)\n' for i in range(1000)
    ]
    in_stream = io.StringIO(''.join(lines))
    out_stream = io.StringIO()

    start_time = time.time()
    symbolize._Symbolize(
        in_stream=in_stream,
        out_stream=out_stream,
        runner=fake_runner,
        base_address='0')
    elapsed = time.time() - start_time

    self.assertLess(
        elapsed, 2.0,
        f'Symbolizing 1,000 frames took {elapsed:.3f}s (expected < 2.0s)')
    self.assertEqual(len(fake_runner.calls), 1000)
    self.assertIn('benchmarked_symbol', out_stream.getvalue())


class SymbolizeIntegrationTests(unittest.TestCase):
  """Integration test compiling a synthetic C shared library on the fly."""

  temp_dir = None
  so_path = None
  symbols = {}

  @classmethod
  def setUpClass(cls):
    if not (os.path.exists(_CLANG) and os.path.exists(_LLVM_NM) and
            os.path.exists(_SYMBOLIZER)):
      return

    cls.temp_dir = tempfile.TemporaryDirectory()
    c_path = os.path.join(cls.temp_dir.name, 'fixture.c')
    cls.so_path = os.path.join(cls.temp_dir.name, 'fixture.so')

    with open(c_path, 'w', encoding='utf-8') as f:
      f.write("""
        int fixture_func_alpha(int a) {
          return a + 42;
        }
        int fixture_func_beta(int b) {
          return fixture_func_alpha(b) * 2;
        }
      """)

    subprocess.check_call(
        [_CLANG, '-shared', '-fPIC', '-g', '-O0', c_path, '-o', cls.so_path])

    nm_out = subprocess.check_output([_LLVM_NM, '-n', cls.so_path], text=True)
    cls.symbols = {}
    for line in nm_out.splitlines():
      parts = line.split()
      if len(parts) >= 3 and parts[2].startswith('fixture_func_'):
        cls.symbols[parts[2]] = hex(int(parts[0], 16))

  @classmethod
  def tearDownClass(cls):
    if cls.temp_dir:
      cls.temp_dir.cleanup()

  def setUp(self):
    if not (os.path.exists(_CLANG) and os.path.exists(_LLVM_NM) and
            os.path.exists(_SYMBOLIZER)):
      self.skipTest('Hermetic LLVM toolchain binaries not found in repository.')

  def test_real_llvm_symbolizer_end_to_end(self):
    alpha_offset = self.symbols['fixture_func_alpha']
    beta_offset = self.symbols['fixture_func_beta']

    input_text = (
        f'Sep 01 08:28:53 Host App[100]: \t<unknown> [{alpha_offset}]\n'
        f'09-08 17:27:44.416  8169  8169 E chromium: #01 pc {beta_offset} '
        f'{self.so_path}\n')

    in_stream = io.StringIO(input_text)
    out_stream = io.StringIO()

    symbolize._Symbolize(
        library=self.so_path,
        base_address='0',
        in_stream=in_stream,
        out_stream=out_stream)

    output = out_stream.getvalue()
    self.assertIn('Sep 01 08:28:53 Host App[100]:', output)
    self.assertIn('fixture_func_alpha', output)
    self.assertIn('fixture_func_beta', output)
    self.assertIn('fixture.c', output)

  def test_real_llvm_symbolizer_with_file_and_base_address(self):
    alpha_offset = int(self.symbols['fixture_func_alpha'], 16)
    base_addr = 0x100000
    loaded_addr = hex(base_addr + alpha_offset)

    with tempfile.NamedTemporaryFile(mode='w+', encoding='utf-8') as f:
      f.write(f'        <unknown> [{loaded_addr}]\n')
      f.flush()
      out_stream = io.StringIO()
      symbolize._Symbolize(
          filename=f.name,
          library=self.so_path,
          base_address=hex(base_addr),
          out_stream=out_stream)
      self.assertIn('fixture_func_alpha', out_stream.getvalue())


if __name__ == '__main__':
  unittest.main()

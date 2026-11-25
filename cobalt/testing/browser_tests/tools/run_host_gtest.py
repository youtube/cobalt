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
"""Host-side runner and XML result converter for Android GTest suites."""

from __future__ import annotations

import argparse
from collections.abc import Sequence
import contextlib
import fcntl
import hashlib
import json
import logging
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import types
from typing import Any
import xml.etree.ElementTree as ET

_BROWSER_SUITES = frozenset(
    {'cobalt_browsertests', 'cobalt_browsertests_loader'})
_FILTER_FLAGS = frozenset({
    '-f',
    '--gtest-filter',
    '--gtest_filter',
    '--test-filter',
    '--isolated-script-test-filter',
})
_ILLEGAL_XML_CHARS_RE = re.compile(r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]')
_XML_SUITE_RE = re.compile(
    r'^(?!(?:out|results?|test_detail)\.xml$)'
    r'(.+?)(?:_shard_?\d+)?(?:_(?:testoutput|results?))?(?:_shard_?\d+)?\.xml$')
_MAX_CACHED_DEPOT_TOOLS = 3
_DEPOT_TOOLS_KEY_FILES = (
    'cipd_manifest.versions',
    'cipd_manifest.txt',
    'python3_bin_reldir.txt',
    'vpython3',
)
_DEFAULT_SRC_DIR = str(Path(__file__).resolve().parents[4])


class RunnerError(Exception):
  """Raised when test setup or execution cannot proceed."""


class _SigtermReceivedError(BaseException):
  """Raised when SIGTERM interrupts the active test runner."""


def infer_suite(explicit_suite: str | None, apk_path: str | None,
                result_xml: str | None,
                passthrough_args: Sequence[str]) -> str | None:
  """Infers the GTest suite name from CLI flags or passthrough arguments."""
  if explicit_suite:
    return explicit_suite
  if apk_path:
    apk_base = os.path.basename(apk_path)
    if apk_base not in ('build.apk', 'test.apk'):
      for suffix in ('-debug_incremental.apk', '-debug.apk', '.apk'):
        if apk_base.endswith(suffix) and len(apk_base) > len(suffix):
          return apk_base[:-len(suffix)]
  if result_xml:
    match = _XML_SUITE_RE.match(os.path.basename(result_xml))
    if match:
      return match.group(1)
  return next((arg for arg in passthrough_args if arg in _BROWSER_SUITES), None)


def _normalize_passthrough_args(raw_args: Sequence[str],
                                explicit_suite: str | None) -> list[str]:
  """Strips wrapper-only tokens and normalizes split filter arguments."""
  pending = list(raw_args)
  normalized: list[str] = []
  while pending:
    arg = pending.pop(0)
    if arg in _BROWSER_SUITES or (explicit_suite and arg == explicit_suite):
      continue
    if arg in ('--gtest_output', '--gtest-output'):
      if pending and not pending[0].startswith('-'):
        pending.pop(0)
      continue
    if arg.startswith(('--gtest_output=', '--gtest-output=')):
      continue
    if arg in _FILTER_FLAGS and pending and not pending[0].startswith('--'):
      normalized.append(f'{arg}={pending.pop(0)}')
      continue
    normalized.append(arg)
  return normalized


def stage_shared_depot_tools(local_depot_tools: str) -> str:
  """Stages depot_tools in a shared host cache for warm vpython3 reuse."""
  if not any(
      Path(local_depot_tools, rel).is_file()
      for rel in _DEPOT_TOOLS_KEY_FILES[:2]):
    return local_depot_tools
  hasher = hashlib.sha256()
  for rel in _DEPOT_TOOLS_KEY_FILES:
    with contextlib.suppress(OSError):
      data = Path(local_depot_tools, rel).read_bytes()
      hasher.update(rel.encode('utf-8') + b'\0' + data + b'\0')
  entry_name = f'depot_tools_{hasher.hexdigest()[:16]}'
  uid, home = os.getuid(), os.path.expanduser('~')
  tmp_cache = os.path.join(tempfile.gettempdir(), f'cobalt_depot_tools.{uid}')
  use_home = home not in ('', '/') and not home.startswith('~')
  default_cache = (
      os.path.join(home, '.cache', f'cobalt_depot_tools.{uid}')
      if use_home else tmp_cache)
  cache_root = os.path.abspath(
      os.environ.get('COBALT_DEPOT_TOOLS_CACHE_DIR') or default_cache)
  shared_dir = shared_vpython = ''
  try:
    try:
      os.makedirs(cache_root, exist_ok=True)
    except OSError:
      cache_root = os.path.abspath(tmp_cache)
      os.makedirs(cache_root, exist_ok=True)
    shared_dir = os.path.join(cache_root, entry_name)
    shared_vpython = os.path.join(shared_dir, 'vpython3')
    if os.path.isfile(shared_vpython):
      with contextlib.suppress(OSError):
        os.utime(shared_dir, None)
      return shared_dir
    with open(os.path.join(cache_root, '.lock'), 'ab') as lock_file:
      fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
      if not os.path.isfile(shared_vpython):
        if os.path.lexists(shared_dir):
          shutil.rmtree(shared_dir, ignore_errors=True)
        with tempfile.TemporaryDirectory(
            prefix='.staging_', dir=cache_root) as staging_parent:
          staging_dir = os.path.join(staging_parent, 'depot_tools')
          shutil.copytree(local_depot_tools, staging_dir, symlinks=True)
          os.replace(staging_dir, shared_dir)
      os.utime(shared_dir, None)
      entries = [(os.stat(os.path.join(cache_root, name)).st_mtime, name)
                 for name in os.listdir(cache_root)
                 if name.startswith('depot_tools_')]
      for _, stale in sorted(entries, reverse=True)[_MAX_CACHED_DEPOT_TOOLS:]:
        if stale != entry_name:
          shutil.rmtree(os.path.join(cache_root, stale), ignore_errors=True)
      return shared_dir
  except OSError as exc:
    logging.warning('Falling back to local depot_tools (%s)', exc)
    return shared_dir if os.path.isfile(shared_vpython) else local_depot_tools


def convert_launcher_summary_to_xml(summary_json: str,
                                    output_file: str,
                                    *,
                                    exit_code: int = 0) -> bool:
  """Converts a Chromium test launcher summary JSON file into GTest XML."""
  with open(summary_json, 'r', encoding='utf-8') as json_file:
    summary_data = json.load(json_file)
  latest_runs: dict[str, dict[str, Any]] = {}
  for iteration_data in summary_data.get('per_iteration_data') or []:
    for full_name, runs in iteration_data.items():
      if runs:
        latest_runs[full_name] = runs[-1]
  for full_name in summary_data.get('all_tests') or []:
    if full_name and full_name not in latest_runs:
      latest_runs[full_name] = {'status': 'NOTRUN', 'elapsed_time_ms': 0}

  grouped: dict[str, list[tuple[str, dict[str, Any]]]] = {}
  for full_name, run_data in latest_runs.items():
    suite_name, _, case_name = full_name.partition('.')
    grouped.setdefault(suite_name, []).append((case_name or
                                               full_name, run_data))

  root = ET.Element('testsuites')
  total_tests = total_failures = total_skipped = total_errors = 0
  total_time = 0.0
  for suite_name, cases in grouped.items():
    suite_el = ET.SubElement(root, 'testsuite', name=suite_name)
    s_failures = s_skipped = 0
    s_time = 0.0
    for case_name, run_data in cases:
      elapsed = round(float(run_data.get('elapsed_time_ms') or 0) / 1000.0, 3)
      s_time += elapsed
      tc_el = ET.SubElement(
          suite_el,
          'testcase',
          classname=suite_name,
          name=case_name,
          status='run',
          time=f'{elapsed:.3f}',
      )
      status = run_data.get('status') or 'UNKNOWN'
      if status == 'SKIPPED':
        s_skipped += 1
        ET.SubElement(tc_el, 'skipped', message='SKIPPED')
      elif status != 'SUCCESS':
        s_failures += 1
        fail_el = ET.SubElement(tc_el, 'failure', message=status, type='')
        snippet = _ILLEGAL_XML_CHARS_RE.sub(
            '',
            run_data.get('output_snippet') or '')
        if snippet:
          fail_el.text = snippet
    suite_el.set('tests', str(len(cases)))
    suite_el.set('failures', str(s_failures))
    suite_el.set('errors', '0')
    suite_el.set('skipped', str(s_skipped))
    suite_el.set('time', f'{s_time:.3f}')
    total_tests += len(cases)
    total_failures += s_failures
    total_skipped += s_skipped
    total_time += s_time

  tags = summary_data.get('global_tags') or ()
  if (exit_code != 0 and
      (total_tests == 0 or (total_failures == 0 and total_errors == 0))) or (
          total_tests == 0 and 'UNRELIABLE_RESULTS' in tags):
    logging.error('Skipping XML %s: exit_code=%d, tests=%d, failures=%d',
                  output_file, exit_code, total_tests, total_failures)
    return False
  root.set('tests', str(total_tests))
  root.set('failures', str(total_failures))
  root.set('errors', str(total_errors))
  root.set('skipped', str(total_skipped))
  root.set('time', f'{total_time:.3f}')
  if os.path.dirname(output_file):
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
  ET.ElementTree(root).write(
      output_file, encoding='utf-8', xml_declaration=True)
  return True


def _stage_apk_symlink(out_dir: str,
                       suite: str,
                       apk_path: str | None,
                       *,
                       required: bool = True) -> None:
  """Ensures <out_dir>/<suite>_apk/<suite>-debug.apk points to the test APK."""
  primary_apk = os.path.join(out_dir, f'{suite}_apk', f'{suite}-debug.apk')
  if not apk_path and os.path.isfile(primary_apk):
    return
  legacy_apk = os.path.join(out_dir, 'apks', f'{suite}.apk')
  src_apk = (
      os.path.abspath(apk_path) if apk_path else
      (os.path.abspath(legacy_apk) if os.path.isfile(legacy_apk) else None))
  if not src_apk or not os.path.isfile(src_apk):
    if required:
      raise RunnerError(f'APK missing or inaccessible for {primary_apk}')
    return
  os.makedirs(os.path.dirname(primary_apk), exist_ok=True)
  if (os.path.lexists(primary_apk) and
      os.path.realpath(primary_apk) != os.path.realpath(src_apk)):
    os.unlink(primary_apk)
  if not os.path.lexists(primary_apk):
    os.symlink(src_apk, primary_apk)


def _terminate_process_group(proc: subprocess.Popen[Any]) -> None:
  """Sends SIGTERM to a child process group, escalating to SIGKILL after 5s."""
  for sig in (signal.SIGTERM, signal.SIGKILL):
    if proc.poll() is not None and getattr(proc, 'returncode',
                                           None) is not None:
      return
    pid = getattr(proc, 'pid', None)
    try:
      if os.name == 'posix' and isinstance(pid, int) and pid > 0:
        os.killpg(pid, sig)
      elif sig == signal.SIGTERM:
        proc.terminate()
      else:
        proc.kill()
      proc.wait(timeout=5)
      return
    except ProcessLookupError:
      return
    except PermissionError:
      if proc.poll() is None:
        (proc.terminate if sig == signal.SIGTERM else proc.kill)()
    except subprocess.TimeoutExpired:
      continue
    except OSError as exc:
      logging.warning('Failed to terminate test runner process: %s', exc)
      return


def _build_parser() -> argparse.ArgumentParser:
  """Builds the argument parser for run_host_gtest."""
  parser = argparse.ArgumentParser(
      description='Run Android GTest suites from a host checkout or toolchain.',
      add_help=False,
      allow_abbrev=False,
  )
  parser.add_argument(
      '--src-dir', default=_DEFAULT_SRC_DIR, help='Path to the src directory.')
  parser.add_argument(
      '--output-dir', default='.', help='Directory for logs and summary files.')
  parser.add_argument('--apk', help='Optional host path to the Android APK.')
  parser.add_argument('--suite', help='Optional explicit GTest suite name.')
  parser.add_argument('--result-xml', help='Optional output XML filename.')
  parser.add_argument('--device-serial', help='Optional adb device serial.')
  return parser


def run_host_gtest(raw_args: Sequence[str]) -> int:
  """Executes the Android host test runner and generates GTest XML output."""
  parser = _build_parser()
  sep = raw_args.index('--') if '--' in raw_args else len(raw_args)
  pre_sep, post_sep = list(raw_args[:sep]), list(raw_args[sep:])
  if '-h' in pre_sep or '--help' in pre_sep:
    parser.add_argument(
        '-h', '--help', action='store_true', help='Show this help message.')
    parser.print_help()
    return 0

  args, raw_passthrough = parser.parse_known_args(pre_sep)
  src_dir = os.path.abspath(args.src_dir)
  output_dir = os.path.abspath(args.output_dir)
  apk_path = os.path.abspath(args.apk) if args.apk else None
  suite = infer_suite(args.suite, apk_path, args.result_xml, raw_passthrough)
  pre_passthrough = _normalize_passthrough_args(raw_passthrough, args.suite)
  summary_json = os.path.join(output_dir, 'test_launcher_summary.json')

  exit_code = 1
  started_runner = False
  in_cleanup = False
  sigterm_received = False
  prev_sigterm = None
  if (os.name == 'posix' and
      threading.current_thread() is threading.main_thread()):

    def _handle_sigterm(signum: int, frame: types.FrameType | None) -> None:
      del signum, frame
      nonlocal sigterm_received
      if not sigterm_received:
        sigterm_received = True
        if not in_cleanup:
          raise _SigtermReceivedError()

    prev_sigterm = signal.signal(signal.SIGTERM, _handle_sigterm)

  try:
    out_parent = os.path.join(src_dir, 'out')
    try:
      configs = sorted(
          d for d in os.listdir(out_parent)
          if os.path.isdir(os.path.join(out_parent, d)))
    except OSError as exc:
      raise RunnerError(f'Missing out directory {out_parent}: {exc}') from exc
    if not configs:
      raise RunnerError(f'No build subdirectory found in {out_parent}')
    out_dir = os.path.join(out_parent, configs[0])

    has_browser_deps = bool(
        list(Path(out_dir).rglob('cobalt_browsertests*.runtime_deps')))
    if (not apk_path and has_browser_deps and
        (suite is None or suite in _BROWSER_SUITES)):
      loader_runner = os.path.join(out_dir, 'bin',
                                   'run_cobalt_browsertests_loader')
      if os.path.isfile(loader_runner):
        runner_script = loader_runner
        wrapper_suite = 'cobalt_browsertests_loader'
      else:
        runner_script = os.path.join(out_dir, 'bin', 'run_cobalt_browsertests')
        wrapper_suite = 'cobalt_browsertests'
      _stage_apk_symlink(out_dir, suite or wrapper_suite, None, required=False)
      cmd_args = list(pre_passthrough)
    else:
      if not suite:
        raise RunnerError('Unable to infer GTest suite; pass --suite or --apk.')
      _stage_apk_symlink(out_dir, suite, apk_path, required=True)
      runner_script = os.path.join(src_dir, 'build', 'android',
                                   'test_runner.py')
      cmd_args = [
          'gtest',
          '--output-directory',
          out_dir,
          '--suite',
          suite,
          '--use-existing-test-data',
      ]
      if '--disable-test-server' not in pre_passthrough:
        cmd_args.append('--disable-test-server')
      has_timeout = any(
          arg in ('-t',
                  '--shard-timeout') or arg.startswith(('--shard-timeout=',
                                                        '-t='))
          for arg in pre_passthrough)
      if suite not in _BROWSER_SUITES and not has_timeout:
        cmd_args.append('--shard-timeout=600')
      cmd_args.extend(pre_passthrough)

    if not os.path.isfile(runner_script):
      raise RunnerError(f'Test runner script not found at {runner_script}')
    with contextlib.suppress(OSError):
      os.chmod(runner_script, 0o755)

    env = os.environ.copy()
    bundled_dt = os.path.join(os.path.dirname(src_dir), 'depot_tools')
    if os.path.isdir(bundled_dt):
      staged_dt = stage_shared_depot_tools(bundled_dt)
      env['PATH'] = staged_dt + os.pathsep + env.get('PATH', '')
    env['DEPOT_TOOLS_UPDATE'] = '0'
    env['CHROME_SRC'] = src_dir
    ld_parts = [out_dir, os.path.join(out_dir, 'starboard')]
    if env.get('LD_LIBRARY_PATH'):
      ld_parts.append(env['LD_LIBRARY_PATH'])
    env['LD_LIBRARY_PATH'] = os.pathsep.join(ld_parts)

    vpython_path = shutil.which('vpython3', path=env.get('PATH'))
    if not vpython_path:
      raise RunnerError('vpython3 not found in bundled depot_tools or PATH.')

    os.makedirs(output_dir, exist_ok=True)
    if os.path.lexists(summary_json):
      os.unlink(summary_json)
    cmd_args.append(f'--json-results-file={summary_json}')
    if args.device_serial:
      cmd_args.extend(['-d', args.device_serial])
    cmd_args.extend(post_sep)

    cmd = [vpython_path, runner_script] + cmd_args
    logging.info('Running command: %s', cmd)
    started_runner = True
    log_path = os.path.join(output_dir, 'test_results.txt')
    with open(log_path, 'a', encoding='utf-8') as log_file:
      with subprocess.Popen(
          cmd,
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
          text=True,
          encoding='utf-8',
          errors='replace',
          env=env,
          start_new_session=(os.name == 'posix'),
      ) as proc:
        try:
          if proc.stdout is None:
            raise RunnerError('Subprocess stdout pipe is not available.')
          for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            log_file.write(line)
            log_file.flush()
          proc.wait()
          exit_code = proc.returncode
        except BaseException:
          if proc.stdout is not None:
            proc.stdout.close()
          _terminate_process_group(proc)
          raise
  except _SigtermReceivedError:
    logging.warning('Received SIGTERM; terminating active test runner.')
    exit_code = 128 + signal.SIGTERM
  except RunnerError as exc:
    logging.error('Runner failed: %s', exc)
    exit_code = 1
  finally:
    in_cleanup = True
    try:
      if started_runner and args.result_xml and os.path.isfile(summary_json):
        xml_out = (
            args.result_xml if os.path.isabs(args.result_xml) else os.path.join(
                output_dir, args.result_xml))
        try:
          convert_launcher_summary_to_xml(
              summary_json, xml_out, exit_code=exit_code)
        except (OSError, ValueError, ET.ParseError) as exc:
          logging.error('Failed to convert %s to XML: %s', summary_json, exc)
    finally:
      if prev_sigterm is not None:
        signal.signal(signal.SIGTERM, prev_sigterm)

  return 128 + signal.SIGTERM if sigterm_received else exit_code


def main() -> int:
  logging.basicConfig(
      level=logging.INFO,
      format='[%(asctime)s] %(levelname)s: %(message)s',
      datefmt='%Y-%m-%d %H:%M:%S',
  )
  return run_host_gtest(sys.argv[1:])


if __name__ == '__main__':
  sys.exit(main())

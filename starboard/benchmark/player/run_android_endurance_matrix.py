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
"""Runs a matrix of SbPlayer Android endurance test cells with resume support.

Reads a JSON list of cell definitions, validates the entire matrix before
touching the target device, and invokes `run_android_endurance.py` once per cell
(or once per `run_1..run_N` when `repeat` is set). Completed cells are skipped
on re-run, while incomplete bundle directories are moved aside under
`<out-dir>/_incomplete/` and re-run without deleting prior artifacts.
"""

import argparse
import csv
import datetime
import fnmatch
import json
import math
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional, Sequence, Tuple

_DEFAULT_ITERATIONS = 1000
_DEFAULT_RUNNER = 'out/android-arm_qa/bin/run_player_benchmarks'
_DEFAULT_STALL_MS = 1500.0
_CELL_NAME_RE = re.compile(r'^[A-Za-z0-9][A-Za-z0-9._-]*$')
_RESERVED_NAMES = frozenset({'_incomplete'})
_VALID_OUTPUT_MODES = frozenset({'punch_out', 'decode_to_texture'})
_VALID_COLOR_MODES = frozenset({'default', 'hdr10', 'bt601', 'abr_bt709_bt601'})
_VALID_ARMS = frozenset({'off', 'on'})

ALLOWED_CELL_KEYS = frozenset({
    'name',
    'iterations',
    'video',
    'audio',
    'gap_ms',
    'seeks',
    'output_mode',
    'color_mode',
    'experimental_features',
    'runner_timeout',
    'repeat',
    'pair',
    'arm',
    'run_if_any_failed',
    'run_if_any_stalled',
})


def _iso_utc_now() -> str:
  return datetime.datetime.now(
      datetime.timezone.utc).isoformat(timespec='seconds')


def _compact_utc_timestamp() -> str:
  return datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')


def _is_strict_int(value: Any) -> bool:
  return isinstance(value, int) and not isinstance(value, bool)


def _validate_earlier_refs(
    *,
    cell_name: str,
    index: int,
    key_name: str,
    refs: Any,
    seen_names: Dict[str, int],
    raw_matrix: Sequence[Any],
) -> None:
  """Validates a `run_if_any_*` list of earlier cell names."""
  if not isinstance(refs, list) or not refs:
    raise ValueError(
        f'Cell {cell_name!r}: "{key_name}" must be a non-empty list of earlier '
        'cell names.')
  for ref in refs:
    if not isinstance(ref, str) or not ref.strip():
      raise ValueError(
          f'Cell {cell_name!r}: "{key_name}" entries must be non-empty '
          'strings.')
    if ref not in seen_names or seen_names[ref] >= index:
      later_names = {
          c.get('name') for c in raw_matrix[index + 1:] if isinstance(c, dict)
      }
      if ref == cell_name or ref in later_names:
        raise ValueError(
            f'Cell {cell_name!r}: "{key_name}" references {ref!r}, which does '
            'not appear earlier in the matrix.')
      raise ValueError(
          f'Cell {cell_name!r}: "{key_name}" references unknown cell {ref!r}.')


def _validate_experimental_features_str(
    cell_name: str,
    features_str: str,
) -> None:
  """Validates Key=<int>(,Key=<int>)* syntax matching C++ flag parsing."""
  if not features_str:
    return
  seen_keys = set()
  for entry in features_str.split(','):
    if '=' not in entry or any(ch.isspace() for ch in entry):
      raise ValueError(
          f'Cell {cell_name!r}: malformed "experimental_features" entry '
          f'{entry!r} (expected Key=<integer>).')
    key, raw_val = entry.split('=', 1)
    if not key or not re.fullmatch(r'[+-]?\d+', raw_val):
      raise ValueError(
          f'Cell {cell_name!r}: malformed "experimental_features" entry '
          f'{entry!r} (expected Key=<integer>).')
    int_val = int(raw_val)
    if int_val < -(1 << 63) or int_val > (1 << 63) - 1:
      raise ValueError(
          f'Cell {cell_name!r}: "experimental_features" value out of int64 '
          f'range in {entry!r}.')
    if key in seen_keys:
      raise ValueError(
          f'Cell {cell_name!r}: duplicate "experimental_features" key {key!r}.')
    seen_keys.add(key)


def validate_matrix(raw_matrix: Any) -> List[Dict[str, Any]]:
  """Validates the parsed matrix JSON list and returns normalized cell dicts."""
  if not isinstance(raw_matrix, list) or not raw_matrix:
    raise ValueError('Matrix JSON must be a non-empty list of cell objects.')

  seen_names: Dict[str, int] = {}
  pairs: Dict[str, List[Tuple[str, str]]] = {}
  validated: List[Dict[str, Any]] = []

  for index, raw_cell in enumerate(raw_matrix):
    if not isinstance(raw_cell, dict):
      raise ValueError(f'Cell at index {index} must be a JSON object.')

    unknown_keys = sorted(set(raw_cell.keys()) - ALLOWED_CELL_KEYS)
    if unknown_keys:
      cell_label = raw_cell.get('name', f'index {index}')
      joined_unknown = ', '.join(unknown_keys)
      raise ValueError(
          f'Cell {cell_label!r} has unknown key(s): {joined_unknown}')

    name = raw_cell.get('name')
    if not isinstance(name, str) or not name.strip() or name != name.strip():
      raise ValueError(
          f'Cell at index {index} must have a non-empty string "name" without '
          'leading or trailing whitespace.')
    if name in _RESERVED_NAMES or not _CELL_NAME_RE.match(name):
      raise ValueError(
          f'Cell name {name!r} is invalid; use alphanumeric, ".", "_", or "-".')
    if name in seen_names:
      raise ValueError(
          f'Duplicate cell name {name!r} at indices {seen_names[name]} and '
          f'{index}.')
    seen_names[name] = index

    if 'iterations' in raw_cell:
      iters = raw_cell['iterations']
      if not _is_strict_int(iters) or iters < 1:
        raise ValueError(
            f'Cell {name!r}: "iterations" must be an integer >= 1.')

    for non_neg_key in (
        'gap_ms',
        'seeks',
        'runner_timeout',
    ):
      if non_neg_key in raw_cell:
        val = raw_cell[non_neg_key]
        if not _is_strict_int(val) or val < 0:
          raise ValueError(
              f'Cell {name!r}: "{non_neg_key}" must be an integer >= 0.')

    if 'video' in raw_cell:
      video = raw_cell['video']
      if not isinstance(video, str) or not video.strip():
        raise ValueError(f'Cell {name!r}: "video" must be a non-empty string.')
      for part in video.split(','):
        if not part or any(ch.isspace() for ch in part):
          raise ValueError(
              f'Cell {name!r}: "video" has empty or whitespace clip entry in '
              f'{video!r}.')

    if 'audio' in raw_cell:
      audio = raw_cell['audio']
      if not isinstance(audio, str) or any(ch.isspace() for ch in audio):
        raise ValueError(
            f'Cell {name!r}: "audio" must be a string without whitespace.')

    if 'output_mode' in raw_cell:
      mode = raw_cell['output_mode']
      if not isinstance(mode, str) or mode not in _VALID_OUTPUT_MODES:
        raise ValueError(f'Cell {name!r}: "output_mode" must be one of '
                         f'{sorted(_VALID_OUTPUT_MODES)}.')

    if 'color_mode' in raw_cell:
      cmode = raw_cell['color_mode']
      if not isinstance(cmode, str) or cmode not in _VALID_COLOR_MODES:
        raise ValueError(f'Cell {name!r}: "color_mode" must be one of '
                         f'{sorted(_VALID_COLOR_MODES)}.')

    if 'experimental_features' in raw_cell:
      exp_features = raw_cell['experimental_features']
      if not isinstance(exp_features, str):
        raise ValueError(
            f'Cell {name!r}: "experimental_features" must be a string.')
      _validate_experimental_features_str(name, exp_features)

    if 'repeat' in raw_cell:
      repeat_val = raw_cell['repeat']
      if not _is_strict_int(repeat_val) or repeat_val < 1:
        raise ValueError(f'Cell {name!r}: "repeat" must be an integer >= 1.')

    has_pair = 'pair' in raw_cell and raw_cell['pair'] is not None
    has_arm = 'arm' in raw_cell and raw_cell['arm'] is not None
    if has_pair != has_arm:
      raise ValueError(
          f'Cell {name!r}: "pair" and "arm" must be specified together.')
    if has_pair:
      pair_name = raw_cell['pair']
      arm_name = raw_cell['arm']
      if (not isinstance(pair_name, str) or not pair_name.strip() or
          pair_name != pair_name.strip() or not _CELL_NAME_RE.match(pair_name)):
        raise ValueError(
            f'Cell {name!r}: "pair" must be a valid identifier using '
            'alphanumeric, ".", "_", or "-".')
      if not isinstance(arm_name, str) or arm_name not in _VALID_ARMS:
        raise ValueError(
            f'Cell {name!r}: "arm" must be "off" or "on", got {arm_name!r}.')
      if 'repeat' in raw_cell:
        raise ValueError(f'Cell {name!r}: cannot combine "pair" with "repeat".')
      pairs.setdefault(pair_name, []).append((arm_name, name))

    for cond_key in ('run_if_any_failed', 'run_if_any_stalled'):
      if cond_key in raw_cell:
        _validate_earlier_refs(
            cell_name=name,
            index=index,
            key_name=cond_key,
            refs=raw_cell[cond_key],
            seen_names=seen_names,
            raw_matrix=raw_matrix,
        )

    validated.append(dict(raw_cell))

  for pair_name, members in pairs.items():
    arms = [arm for arm, _ in members]
    if len(members) != 2 or sorted(arms) != ['off', 'on']:
      member_desc = ', '.join(
          f'{cell_name}({arm})' for arm, cell_name in members)
      raise ValueError(
          f'Pair {pair_name!r} must have exactly one "off" cell and one "on" '
          f'cell; got: {member_desc}.')

  return validated


def load_and_validate_matrix(matrix_path: str) -> List[Dict[str, Any]]:
  """Loads a matrix JSON file from disk and validates it."""
  with open(matrix_path, 'r', encoding='utf-8') as matrix_file:
    raw_data = json.load(matrix_file)
  return validate_matrix(raw_data)


def _parse_optional_float(raw_value: Optional[str]) -> Optional[float]:
  if raw_value is None:
    return None
  stripped = raw_value.strip()
  if not stripped:
    return None
  try:
    return float(stripped)
  except ValueError:
    return None


def _count_authoritative_csv_rows(
    csv_path: str,
    stall_ms: float = _DEFAULT_STALL_MS,
) -> Tuple[int, int, int, int, Dict[str, int], bool]:
  """Counts deduplicated CSV rows, ok/failed/stalled, statuses, hung-stop."""
  if not os.path.isfile(csv_path):
    return 0, 0, 0, 0, {}, False
  with open(csv_path, 'r', encoding='utf-8', errors='replace') as csv_file:
    data_lines = [
        line.strip()
        for line in csv_file
        if line.strip() and not line.strip().startswith('#')
    ]
  if not data_lines:
    return 0, 0, 0, 0, {}, False

  by_iteration: Dict[int, Dict[str, str]] = {}
  for row in csv.DictReader(data_lines):
    try:
      iter_idx = int((row.get('iteration') or '').strip())
    except ValueError:
      continue
    by_iteration[iter_idx] = row

  ordered_iters = sorted(by_iteration)
  status_counts: Dict[str, int] = {}
  ok_count = 0
  stall_count = 0
  for iter_idx in ordered_iters:
    row = by_iteration[iter_idx]
    status = (row.get('status') or 'unknown').strip() or 'unknown'
    status_counts[status] = status_counts.get(status, 0) + 1
    if status == 'ok':
      ok_count += 1
      ctp = _parse_optional_float(row.get('create_to_presenting_ms'))
      dst = _parse_optional_float(row.get('destroy_ms'))
      sk_max = _parse_optional_float(row.get('seek_max_ms'))
      if ((ctp is not None and ctp >= stall_ms) or
          (dst is not None and dst >= stall_ms) or
          (sk_max is not None and sk_max >= stall_ms)):
        stall_count += 1
  row_count = len(ordered_iters)
  is_contiguous = ordered_iters == list(range(row_count))
  last_status = ((by_iteration[ordered_iters[-1]].get('status') or '').strip()
                 if ordered_iters else '')
  hung_early_stop = bool(
      ordered_iters and is_contiguous and last_status.endswith('_hung'))
  return (
      row_count,
      ok_count,
      row_count - ok_count,
      stall_count,
      status_counts,
      hung_early_stop,
  )


def inspect_bundle(
    bundle_dir: str,
    expected_iterations: int,
    stall_ms: float = _DEFAULT_STALL_MS,
) -> Dict[str, Any]:
  """Inspects a bundle directory and classifies whether it is complete."""
  info: Dict[str, Any] = {
      'bundle_dir': bundle_dir,
      'exists': os.path.exists(bundle_dir),
      'complete': False,
      'incomplete_reason': None,
      'expected_iterations': expected_iterations,
      'row_count': 0,
      'ok_count': 0,
      'failed_count': 0,
      'stall_count': 0,
      'status_counts': {},
      'host_exit_code': None,
      'runner_exit_code': None,
      'setup_error': None,
      'runner_timed_out': None,
      'wall_duration_sec': None,
  }
  if not os.path.exists(bundle_dir):
    info['incomplete_reason'] = 'missing'
    return info
  if not os.path.isdir(bundle_dir):
    info['incomplete_reason'] = 'not_a_directory'
    return info

  manifest_path = os.path.join(bundle_dir, 'manifest.json')
  if not os.path.isfile(manifest_path):
    info['incomplete_reason'] = 'missing_manifest'
    return info

  try:
    with open(manifest_path, 'r', encoding='utf-8') as manifest_file:
      manifest = json.load(manifest_file)
  except (OSError, ValueError, json.JSONDecodeError) as exc:
    info['incomplete_reason'] = f'corrupt_manifest: {exc}'
    return info

  if not isinstance(manifest, dict):
    info['incomplete_reason'] = 'manifest_not_object'
    return info

  setup_error = manifest.get('setup_error')
  runner_timed_out = manifest.get('runner_timed_out')
  runner_exit_code = manifest.get('runner_exit_code')
  host_exit_code = manifest.get('host_exit_code')
  manifest_args = manifest.get('args') or {}
  manifest_csv = manifest.get('csv') or {}

  info['setup_error'] = setup_error
  info['runner_timed_out'] = runner_timed_out
  info['runner_exit_code'] = runner_exit_code
  info['host_exit_code'] = host_exit_code
  info['wall_duration_sec'] = manifest.get('wall_duration_sec')

  csv_path = os.path.join(bundle_dir, 'results.csv')
  row_count, ok_count, failed_count, stall_count, status_counts, hung_stop = (
      _count_authoritative_csv_rows(csv_path, stall_ms=stall_ms))
  info['row_count'] = row_count
  info['ok_count'] = ok_count
  info['failed_count'] = failed_count
  info['stall_count'] = stall_count
  info['status_counts'] = status_counts

  if setup_error is not None:
    info['incomplete_reason'] = f'setup_error: {setup_error}'
    return info
  if isinstance(host_exit_code, int) and host_exit_code >= 128:
    info['incomplete_reason'] = f'interrupted_signal_exit_{host_exit_code}'
    return info
  manifest_iters = manifest_args.get('iterations')
  if manifest_iters != expected_iterations:
    info['incomplete_reason'] = (
        f'manifest_iterations_mismatch({manifest_iters} != '
        f'{expected_iterations})')
    return info
  if not manifest_csv.get('csv_present'):
    info['incomplete_reason'] = 'csv_missing_in_manifest'
    return info

  is_valid_hung_stop = (
      hung_stop and 0 < row_count <= expected_iterations and
      manifest_csv.get('row_count') == row_count and
      runner_exit_code is not None)
  if not is_valid_hung_stop:
    if runner_timed_out is not False:
      info['incomplete_reason'] = 'runner_timed_out'
      return info
    if runner_exit_code is None:
      info['incomplete_reason'] = 'runner_not_finished'
      return info
    manifest_rows = manifest_csv.get('row_count')
    if manifest_rows != expected_iterations:
      info['incomplete_reason'] = (
          f'manifest_row_count({manifest_rows}/{expected_iterations})')
      return info
    if row_count != expected_iterations:
      info['incomplete_reason'] = (
          f'csv_row_count({row_count}/{expected_iterations})')
      return info

  info['complete'] = True
  return info


def check_adb_device_ready(
    adb_bin: str,
    serial: str,
    timeout: float = 10.0,
) -> None:
  """Verifies `adb -s <serial> get-state` returns `'device'` or raises."""
  resolved_adb = shutil.which(adb_bin) or adb_bin
  if not os.path.isfile(resolved_adb):
    raise RuntimeError(f'adb binary not found: {adb_bin}')
  try:
    proc = subprocess.run(
        [resolved_adb, '-s', serial, 'get-state'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )
  except subprocess.TimeoutExpired as exc:
    raise RuntimeError(
        f'`adb -s {serial} get-state` timed out after {timeout}s.') from exc
  except OSError as exc:
    raise RuntimeError(
        f'Failed to execute `adb -s {serial} get-state`: {exc}') from exc

  stdout = proc.stdout.decode('utf-8', errors='replace').strip()
  stderr = proc.stderr.decode('utf-8', errors='replace').strip()
  if proc.returncode == 0 and stdout == 'device':
    return

  detail = stderr or stdout or f'rc={proc.returncode}'
  if 'unauthorized' in detail.lower():
    raise RuntimeError(
        f'Device {serial} is not in "device" state ({detail}). Accept the '
        '"Allow USB debugging?" prompt on the device.')
  raise RuntimeError(
      f'Device {serial} is not in "device" state (`adb -s {serial} get-state` '
      f'reported: {detail}).')


def build_cell_command(
    *,
    endurance_script: str,
    args: argparse.Namespace,
    cell: Dict[str, Any],
    bundle_dir: str,
) -> List[str]:
  """Builds the `run_android_endurance.py` command for one bundle run."""
  iterations = int(cell.get('iterations', _DEFAULT_ITERATIONS))
  cmd = [
      sys.executable,
      endurance_script,
      '-d',
      args.serial,
      '--runner',
      args.runner,
      '--bundle-dir',
      bundle_dir,
      '--iterations',
      str(iterations),
  ]
  if 'video' in cell:
    cmd.extend(['--video', str(cell['video'])])
  if 'audio' in cell:
    cmd.extend(['--audio', str(cell['audio'])])
  if 'gap_ms' in cell:
    cmd.extend(['--gap-ms', str(cell['gap_ms'])])
  if 'seeks' in cell:
    cmd.extend(['--seeks', str(cell['seeks'])])
  if 'output_mode' in cell:
    cmd.extend(['--output-mode', str(cell['output_mode'])])
  if 'color_mode' in cell:
    cmd.extend(['--color-mode', str(cell['color_mode'])])
  if 'experimental_features' in cell and cell['experimental_features']:
    cmd.extend(['--experimental-features', str(cell['experimental_features'])])
  if 'runner_timeout' in cell:
    cmd.extend(['--runner-timeout', str(cell['runner_timeout'])])
  if args.reboot_before_each_cell:
    cmd.append('--reboot-before')
  for pkg in args.stop_package:
    cmd.extend(['--stop-package', pkg])
  if args.adb != 'adb':
    cmd.extend(['--adb', args.adb])
  return cmd


def move_incomplete_bundle_aside(
    out_dir: str,
    bundle_dir: str,
    archive_prefix: str,
) -> str:
  """Moves an incomplete bundle directory into `<out-dir>/_incomplete/`."""
  incomplete_root = os.path.join(out_dir, '_incomplete')
  os.makedirs(incomplete_root, exist_ok=True)
  stamp = _compact_utc_timestamp()
  candidate = os.path.join(incomplete_root, f'{archive_prefix}.{stamp}')
  suffix = 1
  while os.path.exists(candidate):
    candidate = os.path.join(incomplete_root,
                             f'{archive_prefix}.{stamp}_{suffix}')
    suffix += 1
  shutil.move(bundle_dir, candidate)
  return candidate


def _prepare_repeat_parent_dir(out_dir: str, cell_name: str) -> Optional[str]:
  """Moves `<out-dir>/<name>` aside if it holds flat non-repeat artifacts."""
  cell_dir = os.path.join(out_dir, cell_name)
  if not os.path.exists(cell_dir):
    return None
  if not os.path.isdir(cell_dir):
    return move_incomplete_bundle_aside(out_dir, cell_dir, cell_name)
  entries = os.listdir(cell_dir)
  if not entries:
    return None
  has_non_run_entries = any(
      not re.match(r'^run_\d+$', entry) for entry in entries)
  if has_non_run_entries:
    return move_incomplete_bundle_aside(out_dir, cell_dir, cell_name)
  return None


def _evaluate_prerequisite_cell(
    out_dir: str,
    cell: Dict[str, Any],
    recorded_cells: Dict[str, Dict[str, Any]],
    stall_ms: float,
) -> Tuple[str, int, int]:
  """Returns `(state, failed_count, stall_count)` for a prerequisite cell.

  `state` is one of `'complete'`, `'not_triggered'`, or `'incomplete'`.
  """
  name = cell['name']
  expected_iters = int(cell.get('iterations', _DEFAULT_ITERATIONS))
  repeat_count = cell.get('repeat')

  recorded = recorded_cells.get(name)
  if recorded is not None:
    status = recorded.get('status', '')
    if status == 'not_triggered':
      return 'not_triggered', 0, 0
    if status in ('complete_ok', 'complete_with_failures'):
      return (
          'complete',
          int(recorded.get('failed_count', 0)),
          int(recorded.get('stall_count', 0)),
      )
    return (
        'incomplete',
        int(recorded.get('failed_count', 0)),
        int(recorded.get('stall_count', 0)),
    )

  # Fallback to inspecting bundles on disk (for example when `--only` is used).
  def _prior_state_not_triggered() -> bool:
    state_path = os.path.join(out_dir, 'matrix_state.json')
    if not os.path.isfile(state_path):
      return False
    try:
      with open(state_path, 'r', encoding='utf-8') as state_file:
        prior = json.load(state_file)
      prior_cell = (prior.get('cells') or {}).get(name) or {}
      return prior_cell.get('status') == 'not_triggered'
    except (OSError, ValueError, json.JSONDecodeError):
      return False

  if repeat_count is None:
    b_info = inspect_bundle(
        os.path.join(out_dir, name), expected_iters, stall_ms=stall_ms)
    if b_info['complete']:
      return 'complete', int(b_info['failed_count']), int(b_info['stall_count'])
    if not b_info['exists'] and _prior_state_not_triggered():
      return 'not_triggered', 0, 0
    return 'incomplete', 0, 0

  total_failed = 0
  total_stalled = 0
  any_run_exists = False
  all_runs_complete = True
  for run_idx in range(1, int(repeat_count) + 1):
    run_dir = os.path.join(out_dir, name, f'run_{run_idx}')
    b_info = inspect_bundle(run_dir, expected_iters, stall_ms=stall_ms)
    if b_info['exists']:
      any_run_exists = True
    if not b_info['complete']:
      all_runs_complete = False
    else:
      total_failed += int(b_info['failed_count'])
      total_stalled += int(b_info['stall_count'])
  if all_runs_complete:
    return 'complete', total_failed, total_stalled
  if not any_run_exists and _prior_state_not_triggered():
    return 'not_triggered', 0, 0
  return 'incomplete', total_failed, total_stalled


class MatrixLogger:
  """Writes timestamped lines to both stdout and `<out-dir>/matrix.log`."""

  def __init__(self, log_path: str):
    self.log_path = log_path

  def log(self, message: str) -> None:
    line = f'[{_iso_utc_now()}] {message}'
    print(line, flush=True)
    with open(self.log_path, 'a', encoding='utf-8') as log_file:
      log_file.write(line + '\n')


def _write_state_file(state_path: str, state: Dict[str, Any]) -> None:
  state['updated_at_utc'] = _iso_utc_now()
  tmp_path = f'{state_path}.tmp'
  with open(tmp_path, 'w', encoding='utf-8') as out_file:
    json.dump(state, out_file, indent=2, sort_keys=False, allow_nan=False)
    out_file.write('\n')
  os.replace(tmp_path, state_path)


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description=(
          'Run a resumable matrix of SbPlayer Android endurance test cells.'))
  parser.add_argument(
      '--matrix', required=True, help='Path to the matrix JSON file.')
  parser.add_argument(
      '-d',
      '--serial',
      '--device',
      dest='serial',
      required=True,
      help='Target adb device serial.')
  parser.add_argument(
      '--out-dir',
      required=True,
      help='Output directory for cell bundles, matrix_state.json, and logs.')
  parser.add_argument(
      '--runner',
      default=_DEFAULT_RUNNER,
      help='Path to the generated run_player_benchmarks wrapper script.')
  parser.add_argument(
      '--adb', default='adb', help='Path to the adb binary (default: adb).')
  parser.add_argument(
      '--stop-package',
      action='append',
      default=[],
      help='Package name(s) (repeatable or comma-separated) to force-stop.')
  parser.add_argument(
      '--reboot-before-each-cell',
      action='store_true',
      help='Reboot the device and wait for boot before each cell execution.')
  parser.add_argument(
      '--stall-ms',
      type=float,
      default=_DEFAULT_STALL_MS,
      help=('Latency threshold in ms on ok rows (create_to_presenting_ms, '
            'seek_max_ms, or destroy_ms) for run_if_any_stalled '
            '(default: 1500).'))
  parser.add_argument(
      '--only',
      default=None,
      help='Optional fnmatch glob to run only matching cell names.')
  parser.add_argument(
      '--dry-run',
      action='store_true',
      help='Validate matrix JSON and print per-cell commands; touch nothing.')
  parser.add_argument(
      '--on-failure',
      choices=('continue', 'stop'),
      default='continue',
      help=('Action when a cell records failed iterations or is incomplete '
            '(default: continue).'))
  args = parser.parse_args(argv)
  if not math.isfinite(args.stall_ms) or args.stall_ms <= 0:
    parser.error('--stall-ms must be finite and > 0')
  return args


def main(argv: Optional[Sequence[str]] = None) -> int:
  args = _parse_args(argv if argv is not None else sys.argv[1:])

  try:
    cells = load_and_validate_matrix(args.matrix)
  except (OSError, ValueError, json.JSONDecodeError) as exc:
    print(f'[matrix] Invalid matrix {args.matrix}: {exc}', file=sys.stderr)
    return 2

  cells_by_name = {cell['name']: cell for cell in cells}
  if args.only is not None:
    selected_cells = [
        cell for cell in cells if fnmatch.fnmatchcase(cell['name'], args.only)
    ]
    if not selected_cells:
      print(
          f'[matrix] No cells matched --only pattern {args.only!r}.',
          file=sys.stderr,
      )
      return 2
  else:
    selected_cells = list(cells)

  endurance_script = os.path.join(
      os.path.dirname(os.path.abspath(__file__)), 'run_android_endurance.py')
  out_dir = os.path.abspath(args.out_dir)

  if args.dry_run:
    for cell in selected_cells:
      name = cell['name']
      cond_parts: List[str] = []
      if 'run_if_any_failed' in cell:
        failed_refs = cell['run_if_any_failed']
        cond_parts.append(f'run_if_any_failed={failed_refs!r}')
      if 'run_if_any_stalled' in cell:
        stalled_refs = cell['run_if_any_stalled']
        cond_parts.append(f'run_if_any_stalled={stalled_refs!r}')
      joined_cond = ', '.join(cond_parts)
      cond_prefix = f'# [conditional: {joined_cond}]\n' if cond_parts else ''
      repeat_count = cell.get('repeat')
      if repeat_count is None:
        cmd = build_cell_command(
            endurance_script=endurance_script,
            args=args,
            cell=cell,
            bundle_dir=os.path.join(out_dir, name),
        )
        print(f'{cond_prefix}{shlex.join(cmd)}')
      else:
        for run_idx in range(1, int(repeat_count) + 1):
          cmd = build_cell_command(
              endurance_script=endurance_script,
              args=args,
              cell=cell,
              bundle_dir=os.path.join(out_dir, name, f'run_{run_idx}'),
          )
          print(f'{cond_prefix}{shlex.join(cmd)}')
    return 0

  try:
    check_adb_device_ready(args.adb, args.serial)
  except RuntimeError as exc:
    print(f'[matrix] Error: {exc}', file=sys.stderr)
    return 2

  os.makedirs(out_dir, exist_ok=True)
  log_path = os.path.join(out_dir, 'matrix.log')
  state_path = os.path.join(out_dir, 'matrix_state.json')
  logger = MatrixLogger(log_path)

  prior_cells: Dict[str, Any] = {}
  if os.path.isfile(state_path):
    try:
      with open(state_path, 'r', encoding='utf-8') as prev_file:
        prev_data = json.load(prev_file)
      if isinstance(prev_data, dict) and isinstance(
          prev_data.get('cells'), dict):
        prior_cells = dict(prev_data['cells'])
    except (OSError, ValueError, json.JSONDecodeError):
      prior_cells = {}

  state: Dict[str, Any] = {
      'matrix_path': os.path.abspath(args.matrix),
      'out_dir': out_dir,
      'serial': args.serial,
      'runner': args.runner,
      'on_failure': args.on_failure,
      'stall_ms': args.stall_ms,
      'only': args.only,
      'reboot_before_each_cell': args.reboot_before_each_cell,
      'started_at_utc': _iso_utc_now(),
      'updated_at_utc': _iso_utc_now(),
      'cells': prior_cells,
  }
  _write_state_file(state_path, state)
  logger.log(
      f'Starting matrix ({len(selected_cells)}/{len(cells)} cells selected) '
      f'on {args.serial} -> {out_dir}')

  session_cells: Dict[str, Dict[str, Any]] = {}
  halted_early = False
  adb_abort_error: Optional[str] = None

  for cell in selected_cells:
    if halted_early or adb_abort_error is not None:
      session_cells[cell['name']] = {
          'name':
              cell['name'],
          'status':
              'not_run',
          'reason':
              'sweep_halted_early',
          'expected_iterations':
              int(cell.get('iterations', _DEFAULT_ITERATIONS)),
          'row_count':
              0,
          'ok_count':
              0,
          'failed_count':
              0,
          'stall_count':
              0,
      }
      state['cells'][cell['name']] = session_cells[cell['name']]
      _write_state_file(state_path, state)
      continue

    name = cell['name']
    expected_iters = int(cell.get('iterations', _DEFAULT_ITERATIONS))
    refs_failed = cell.get('run_if_any_failed') or []
    refs_stalled = cell.get('run_if_any_stalled') or []
    if refs_failed or refs_stalled:
      all_refs: List[str] = []
      for ref_name in list(refs_failed) + list(refs_stalled):
        if ref_name not in all_refs:
          all_refs.append(ref_name)

      prereq_incomplete: List[str] = []
      prereq_counts: Dict[str, Tuple[int, int]] = {}
      for ref_name in all_refs:
        ref_state, ref_failed, ref_stalled = _evaluate_prerequisite_cell(
            out_dir,
            cells_by_name[ref_name],
            session_cells,
            args.stall_ms,
        )
        if ref_state == 'incomplete':
          prereq_incomplete.append(ref_name)
        else:
          prereq_counts[ref_name] = (ref_failed, ref_stalled)

      if prereq_incomplete:
        joined_prereqs = ', '.join(prereq_incomplete)
        msg = (f'Cell {name}: prerequisite cell(s) incomplete: '
               f'{joined_prereqs}')
        logger.log(f'INCOMPLETE {msg}')
        record = {
            'name': name,
            'status': 'incomplete',
            'incomplete_reason': msg,
            'run_if_any_failed': refs_failed or None,
            'run_if_any_stalled': refs_stalled or None,
            'expected_iterations': expected_iters,
            'row_count': 0,
            'ok_count': 0,
            'failed_count': 0,
            'stall_count': 0,
        }
        session_cells[name] = record
        state['cells'][name] = record
        _write_state_file(state_path, state)
        if args.on_failure == 'stop':
          halted_early = True
        continue

      trigger_reasons: List[str] = []
      for ref_name in refs_failed:
        failed_cnt, _ = prereq_counts.get(ref_name, (0, 0))
        if failed_cnt > 0:
          trigger_reasons.append(f'{ref_name}(failed={failed_cnt})')
      for ref_name in refs_stalled:
        _, stalled_cnt = prereq_counts.get(ref_name, (0, 0))
        if stalled_cnt > 0:
          trigger_reasons.append(f'{ref_name}(stalled={stalled_cnt})')

      if not trigger_reasons:
        logger.log(f'SKIP {name}: not_triggered (no matching failures/stalls)')
        record = {
            'name': name,
            'status': 'not_triggered',
            'run_if_any_failed': refs_failed or None,
            'run_if_any_stalled': refs_stalled or None,
            'expected_iterations': expected_iters,
            'row_count': 0,
            'ok_count': 0,
            'failed_count': 0,
            'stall_count': 0,
        }
        session_cells[name] = record
        state['cells'][name] = record
        _write_state_file(state_path, state)
        continue

      joined_triggers = ', '.join(trigger_reasons)
      logger.log(f'TRIGGERED {name}: condition matched {joined_triggers}')

    repeat_count = cell.get('repeat')
    moved_archives: List[str] = []
    if repeat_count is not None:
      moved_parent = _prepare_repeat_parent_dir(out_dir, name)
      if moved_parent is not None:
        moved_archives.append(moved_parent)
        logger.log(f'MOVED incomplete parent {name} -> {moved_parent}')
      os.makedirs(os.path.join(out_dir, name), exist_ok=True)
      units = [(
          f'run_{idx}',
          os.path.join(out_dir, name, f'run_{idx}'),
          f'{name}.run_{idx}',
      ) for idx in range(1,
                         int(repeat_count) + 1)]
    else:
      units = [(None, os.path.join(out_dir, name), name)]

    unit_results: List[Dict[str, Any]] = []
    executed_new_run = False
    for unit_label, bundle_dir, archive_prefix in units:
      display_label = f'{name}/{unit_label}' if unit_label else name
      existing = inspect_bundle(
          bundle_dir, expected_iters, stall_ms=args.stall_ms)
      if existing['complete']:
        ex_rows = existing['row_count']
        ex_ok = existing['ok_count']
        ex_failed = existing['failed_count']
        ex_stalled = existing['stall_count']
        logger.log(f'SKIP {display_label}: already complete '
                   f'(rows={ex_rows}/{expected_iters}, ok={ex_ok}, '
                   f'failed={ex_failed}, stalled={ex_stalled})')
        existing['resumed'] = True
        unit_results.append(existing)
        continue

      if existing['exists']:
        moved_to = move_incomplete_bundle_aside(out_dir, bundle_dir,
                                                archive_prefix)
        moved_archives.append(moved_to)
        ex_reason = existing['incomplete_reason']
        logger.log(f'MOVED incomplete bundle {display_label} '
                   f'({ex_reason}) -> {moved_to}')

      try:
        check_adb_device_ready(args.adb, args.serial)
      except RuntimeError as exc:
        adb_abort_error = str(exc)
        logger.log(f'FATAL before {display_label}: {adb_abort_error}')
        unit_results.append({
            'bundle_dir': bundle_dir,
            'complete': False,
            'resumed': False,
            'incomplete_reason': f'device_not_ready: {adb_abort_error}',
            'expected_iterations': expected_iters,
            'row_count': 0,
            'ok_count': 0,
            'failed_count': 0,
            'stall_count': 0,
            'status_counts': {},
            'wall_duration_sec': 0.0,
        })
        break

      cmd = build_cell_command(
          endurance_script=endurance_script,
          args=args,
          cell=cell,
          bundle_dir=bundle_dir,
      )
      logger.log(f'RUN {display_label}: {shlex.join(cmd)}')
      start_mono = time.monotonic()
      proc = subprocess.run(cmd, check=False)
      elapsed_sec = round(time.monotonic() - start_mono, 3)
      executed_new_run = True

      post = inspect_bundle(bundle_dir, expected_iters, stall_ms=args.stall_ms)
      post['resumed'] = False
      post['subprocess_returncode'] = proc.returncode
      if post['wall_duration_sec'] is None:
        post['wall_duration_sec'] = elapsed_sec
      unit_results.append(post)

      p_ok = post['ok_count']
      p_failed = post['failed_count']
      p_stalled = post['stall_count']
      p_rows = post['row_count']
      p_wall = post['wall_duration_sec']
      p_statuses = post['status_counts']
      p_reason = post['incomplete_reason']
      if post['complete']:
        if p_failed == 0:
          logger.log(f'DONE {display_label}: complete_ok '
                     f'(ok={p_ok}/{expected_iters}, stalled={p_stalled}, '
                     f'wall={p_wall}s)')
        else:
          logger.log(f'DONE {display_label}: complete_with_failures '
                     f'(ok={p_ok}/{expected_iters}, failed={p_failed}, '
                     f'stalled={p_stalled}, statuses={p_statuses})')
          if args.on_failure == 'stop':
            halted_early = True
            break
      else:
        logger.log(f'INCOMPLETE {display_label}: did not run to completion '
                   f'(reason={p_reason}, rows={p_rows}/{expected_iters}, '
                   f'rc={proc.returncode})')
        if args.on_failure == 'stop':
          halted_early = True
          break

    all_units_complete = (
        len(unit_results) == len(units) and
        all(u['complete'] for u in unit_results))
    total_rows = sum(int(u['row_count']) for u in unit_results)
    total_ok = sum(int(u['ok_count']) for u in unit_results)
    total_failed = sum(int(u['failed_count']) for u in unit_results)
    total_stalled = sum(int(u.get('stall_count', 0)) for u in unit_results)
    merged_statuses: Dict[str, int] = {}
    for u in unit_results:
      for status_key, count in (u.get('status_counts') or {}).items():
        merged_statuses[status_key] = (
            merged_statuses.get(status_key, 0) + int(count))

    if all_units_complete:
      cell_status = ('complete_with_failures'
                     if total_failed > 0 else 'complete_ok')
      incomplete_reason = None
    else:
      cell_status = 'incomplete'
      reasons = [
          str(u.get('incomplete_reason'))
          for u in unit_results
          if not u.get('complete')
      ]
      if len(unit_results) < len(units):
        reasons.append(f'ran_{len(unit_results)}_of_{len(units)}_runs')
      incomplete_reason = '; '.join(reasons)

    cell_record: Dict[str, Any] = {
        'name': name,
        'status': cell_status,
        'incomplete_reason': incomplete_reason,
        'resumed_all': all(bool(u.get('resumed')) for u in unit_results),
        'executed_new_run': executed_new_run,
        'repeat': repeat_count,
        'pair': cell.get('pair'),
        'arm': cell.get('arm'),
        'expected_iterations_per_run': expected_iters,
        'row_count': total_rows,
        'ok_count': total_ok,
        'failed_count': total_failed,
        'stall_count': total_stalled,
        'status_counts': merged_statuses,
        'moved_incomplete_bundles': moved_archives,
    }
    if repeat_count is not None:
      cell_record['runs'] = unit_results
    else:
      cell_record['bundle_dir'] = units[0][1]

    session_cells[name] = cell_record
    state['cells'][name] = cell_record
    _write_state_file(state_path, state)

  complete_ok_cells = [
      name for name, rec in session_cells.items()
      if rec['status'] == 'complete_ok'
  ]
  failed_iter_cells = [(name, rec['failed_count'], rec['row_count'])
                       for name, rec in session_cells.items()
                       if rec['status'] == 'complete_with_failures']
  stalled_cells = [
      (name, rec.get('stall_count', 0), rec['row_count'])
      for name, rec in session_cells.items()
      if rec['status'] in ('complete_ok', 'complete_with_failures') and
      rec.get('stall_count', 0) > 0
  ]
  not_triggered_cells = [
      name for name, rec in session_cells.items()
      if rec['status'] == 'not_triggered'
  ]
  incomplete_cells = [
      (name, rec.get('incomplete_reason') or rec.get('reason') or rec['status'])
      for name, rec in session_cells.items()
      if rec['status'] not in ('complete_ok', 'complete_with_failures',
                               'not_triggered')
  ]

  state['summary'] = {
      'selected_cell_count': len(selected_cells),
      'complete_ok_count': len(complete_ok_cells),
      'complete_with_failures_count': len(failed_iter_cells),
      'stalled_cell_count': len(stalled_cells),
      'not_triggered_count': len(not_triggered_cells),
      'incomplete_count': len(incomplete_cells),
      'failed_iterations_by_cell': {
          name: failed for name, failed, _ in failed_iter_cells
      },
      'stalled_iterations_by_cell': {
          name: stalled for name, stalled, _ in stalled_cells
      },
      'incomplete_cells': dict(incomplete_cells),
  }
  _write_state_file(state_path, state)

  if failed_iter_cells:
    failed_desc = ', '.join(
        f'{name}={failed}/{rows}' for name, failed, rows in failed_iter_cells)
  else:
    failed_desc = 'none (0 failed iterations)'
  if stalled_cells:
    stalled_desc = ', '.join(
        f'{name}={stalled}/{rows}' for name, stalled, rows in stalled_cells)
  else:
    stalled_desc = 'none'

  logger.log(f'Matrix summary: selected={len(selected_cells)}, '
             f'complete_ok={len(complete_ok_cells)}, '
             f'complete_with_failures={len(failed_iter_cells)}, '
             f'not_triggered={len(not_triggered_cells)}, '
             f'incomplete={len(incomplete_cells)} | '
             f'failed_iterations: {failed_desc} | '
             f'stalled_iterations(>={args.stall_ms:g}ms): {stalled_desc}')

  if incomplete_cells:
    inc_desc = ', '.join(f'{name} ({why})' for name, why in incomplete_cells)
    print(
        f'[matrix] Incomplete cells ({len(incomplete_cells)}): {inc_desc}',
        file=sys.stderr,
    )
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())

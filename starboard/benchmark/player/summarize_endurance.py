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
"""Summarizes SbPlayer Android endurance bundles into JSON and Markdown reports.

Reads a matrix JSON file and a directory of collected bundles, computes per-cell
steady-state latency distributions, within-cell startup/teardown drift, periodic
package PSS deltas, per-cell threshold findings, and optional paired ON minus
OFF comparisons when pairs are declared.
"""

import argparse
import csv
import glob
import json
import math
import os
import re
import statistics
import sys
from typing import Any, Dict, List, Optional, Sequence, Tuple

BASE_METRICS = ('create_ms', 'create_to_presenting_ms', 'destroy_ms', 'gap_ms')
SEEK_METRICS = ('seek_median_ms', 'seek_max_ms')
ALL_METRICS = BASE_METRICS + SEEK_METRICS

DEFAULT_STALL_MS = 1500.0
DEFAULT_PSS_GROWTH_MB = 20.0
DEFAULT_DRIFT_MS = 50.0
DEFAULT_DRIFT_PCT = 25.0

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


def percentile_nearest_rank(values: Sequence[float], pct: float) -> float:
  """Nearest-rank percentile matching the C++ endurance harness."""
  if not values:
    return float('nan')
  ordered = sorted(values)
  idx = max(0,
            min(len(ordered) - 1,
                math.ceil((pct / 100.0) * len(ordered)) - 1))
  return ordered[idx]


def load_authoritative_csv_rows(
    csv_path: str,) -> Tuple[int, List[Dict[str, Any]]]:
  """Returns `(raw_row_count, deduplicated_rows_by_iteration)`."""
  if not os.path.isfile(csv_path):
    return 0, []
  with open(csv_path, 'r', encoding='utf-8', errors='replace') as csv_file:
    data_lines = [
        line.strip()
        for line in csv_file
        if line.strip() and not line.strip().startswith('#')
    ]
  if not data_lines:
    return 0, []

  raw_count = 0
  by_iter: Dict[int, Dict[str, Any]] = {}
  for raw in csv.DictReader(data_lines):
    raw_count += 1
    try:
      iter_idx = int((raw.get('iteration') or '').strip())
    except ValueError:
      continue
    parsed: Dict[str, Any] = dict(raw)
    parsed['iter_int'] = iter_idx
    try:
      parsed['seeks_completed_int'] = int(
          (raw.get('seeks_completed') or '0').strip() or '0')
    except ValueError:
      parsed['seeks_completed_int'] = 0
    for col in ALL_METRICS + ('create_to_initialized_ms',):
      val_str = (raw.get(col) or '').strip()
      parsed[col] = float(val_str) if val_str != '' else None
    by_iter[iter_idx] = parsed
  return raw_count, [by_iter[k] for k in sorted(by_iter)]


def summarize_metric(
    rows: Sequence[Dict[str, Any]],
    col: str,
) -> Dict[str, Optional[float]]:
  """Computes summary statistics over `status == 'ok'` rows for `col`."""
  vals = [
      r[col] for r in rows if r.get('status') == 'ok' and r.get(col) is not None
  ]
  if not vals:
    return {
        'n': 0,
        'mean': None,
        'median': None,
        'p95': None,
        'p99': None,
        'sd': None,
        'min': None,
        'max': None,
    }
  return {
      'n': len(vals),
      'mean': round(statistics.mean(vals), 3),
      'median': round(statistics.median(vals), 3),
      'p95': round(percentile_nearest_rank(vals, 95.0), 3),
      'p99': round(percentile_nearest_rank(vals, 99.0), 3),
      'sd': round(statistics.stdev(vals), 3) if len(vals) >= 2 else 0.0,
      'min': round(min(vals), 3),
      'max': round(max(vals), 3),
  }


_MEMINFO_PROC_HEADER_RE = re.compile(
    r'\*\* MEMINFO in pid (\d+) \[([^\]]+)\] \*\*')


def _parse_pss_from_section(section: str) -> Optional[int]:
  match = re.search(r'TOTAL PSS:\s+(\d+)', section)
  if match:
    return int(match.group(1))
  match_table = re.search(r'^\s*TOTAL\s+(\d+)\s+', section, flags=re.MULTILINE)
  if match_table:
    return int(match_table.group(1))
  return None


def _parse_meminfo_snapshot(path: str) -> Optional[Tuple[str, int, int]]:
  """Returns `(process_name, pid, total_pss_kb)` from a meminfo snapshot."""
  if not os.path.isfile(path):
    return None
  with open(path, 'r', encoding='utf-8', errors='replace') as mem_file:
    text = mem_file.read()

  headers = list(_MEMINFO_PROC_HEADER_RE.finditer(text))
  if not headers:
    pss_kb = _parse_pss_from_section(text)
    return ('', 0, pss_kb) if pss_kb is not None else None

  blocks: List[Tuple[str, int, int]] = []
  for idx, hdr in enumerate(headers):
    start = hdr.end()
    end = headers[idx + 1].start() if idx + 1 < len(headers) else len(text)
    pss_kb = _parse_pss_from_section(text[start:end])
    if pss_kb is not None:
      blocks.append((hdr.group(2), int(hdr.group(1)), pss_kb))

  if not blocks:
    return None
  for proc_name, pid, pss_kb in blocks:
    if proc_name.endswith(':test_process'):
      return proc_name, pid, pss_kb
  return blocks[0]


def parse_meminfo_pss_kb(path: str) -> Optional[int]:
  """Extracts TOTAL PSS in KB from an `adb shell dumpsys meminfo` snapshot."""
  parsed = _parse_meminfo_snapshot(path)
  return parsed[2] if parsed is not None else None


def extract_periodic_pss(bundle_dir: str) -> Dict[str, Any]:
  """Extracts first-vs-last periodic PSS snapshot within the test process."""
  dumpsys_dir = os.path.join(bundle_dir, 'dumpsys')
  raw_candidates: List[Tuple[int, str, int, int]] = []
  if os.path.isdir(dumpsys_dir):
    pattern = os.path.join(dumpsys_dir, 'iter_*_meminfo_pkg.txt')
    for path in glob.glob(pattern):
      match = re.search(r'iter_(\d+)_meminfo_pkg\.txt$', os.path.basename(path))
      if not match:
        continue
      snap = _parse_meminfo_snapshot(path)
      if snap is not None:
        proc_name, pid, pss_kb = snap
        raw_candidates.append((int(match.group(1)), proc_name, pid, pss_kb))

  raw_candidates.sort(key=lambda item: item[0])
  if any(proc.endswith(':test_process') for _, proc, _, _ in raw_candidates):
    raw_candidates = [
        item for item in raw_candidates if item[1].endswith(':test_process')
    ]

  if raw_candidates:
    _, target_proc, target_pid, _ = raw_candidates[-1]
    candidates = [(it, pss_kb)
                  for it, proc, pid, pss_kb in raw_candidates
                  if proc == target_proc and pid == target_pid]
  else:
    candidates = []

  if not candidates:
    return {
        'start_iter': None,
        'start_kb': None,
        'start_mb': None,
        'end_iter': None,
        'end_kb': None,
        'end_mb': None,
        'delta_kb': None,
        'delta_mb': None,
    }

  start_iter, start_kb = candidates[0]
  if len(candidates) < 2:
    return {
        'start_iter': start_iter,
        'start_kb': start_kb,
        'start_mb': round(start_kb / 1024.0, 2),
        'end_iter': None,
        'end_kb': None,
        'end_mb': None,
        'delta_kb': None,
        'delta_mb': None,
    }

  end_iter, end_kb = candidates[-1]
  delta_kb = end_kb - start_kb
  return {
      'start_iter': start_iter,
      'start_kb': start_kb,
      'start_mb': round(start_kb / 1024.0, 2),
      'end_iter': end_iter,
      'end_kb': end_kb,
      'end_mb': round(end_kb / 1024.0, 2),
      'delta_kb': delta_kb,
      'delta_mb': round(delta_kb / 1024.0, 2),
  }


def analyze_bundle(
    bundle_dir: str,
    display_name: str,
    cell_spec: Dict[str, Any],
    stall_ms: float = DEFAULT_STALL_MS,
) -> Dict[str, Any]:
  """Analyzes a single endurance bundle directory."""
  manifest_path = os.path.join(bundle_dir, 'manifest.json')
  manifest: Dict[str, Any] = {}
  valid_manifest = False
  if os.path.isfile(manifest_path):
    try:
      with open(manifest_path, 'r', encoding='utf-8') as manifest_file:
        loaded = json.load(manifest_file)
      if isinstance(loaded, dict):
        manifest = loaded
        valid_manifest = True
    except (OSError, ValueError, json.JSONDecodeError):
      manifest = {}

  manifest_args = manifest.get('args') or {}
  expected_iters = int(cell_spec.get('iterations', 1000))
  seeks_per_iter = int(
      cell_spec.get('seeks') if 'seeks' in cell_spec else (
          manifest_args.get('seeks') or 0))
  requested_gap_ms = int(
      cell_spec.get('gap_ms') if 'gap_ms' in cell_spec else (
          manifest_args.get('gap_ms') or 0))

  raw_count, rows = load_authoritative_csv_rows(
      os.path.join(bundle_dir, 'results.csv'))
  row_count = len(rows)
  status_counts: Dict[str, int] = {}
  error_breakdown: Dict[str, int] = {}
  total_seeks_completed = 0
  stall_count = 0
  max_ok_ctp_ms: Optional[float] = None
  max_ok_destroy_ms: Optional[float] = None
  max_ok_seek_ms: Optional[float] = None

  for row in rows:
    status = (row.get('status') or 'unknown').strip() or 'unknown'
    status_counts[status] = status_counts.get(status, 0) + 1
    total_seeks_completed += int(row.get('seeks_completed_int', 0))
    if status == 'ok':
      ctp = row.get('create_to_presenting_ms')
      dst = row.get('destroy_ms')
      sk_max = row.get('seek_max_ms')
      if ctp is not None:
        max_ok_ctp_ms = ctp if max_ok_ctp_ms is None else max(
            max_ok_ctp_ms, ctp)
      if dst is not None:
        max_ok_destroy_ms = dst if max_ok_destroy_ms is None else max(
            max_ok_destroy_ms, dst)
      if sk_max is not None:
        max_ok_seek_ms = sk_max if max_ok_seek_ms is None else max(
            max_ok_seek_ms, sk_max)
      if ((ctp is not None and ctp >= stall_ms) or
          (dst is not None and dst >= stall_ms) or
          (sk_max is not None and sk_max >= stall_ms)):
        stall_count += 1
    else:
      ecode = (row.get('error_code') or '').strip()
      key = f'{status}:{ecode}' if ecode else status
      error_breakdown[key] = error_breakdown.get(key, 0) + 1

  ok_count = status_counts.get('ok', 0)
  failed_count = row_count - ok_count

  cold_row = next((r for r in rows if r['iter_int'] == 0), None)
  cold_ctp_ms = (
      round(cold_row['create_to_presenting_ms'], 3) if
      (cold_row and
       cold_row.get('create_to_presenting_ms') is not None) else None)
  cold_status = cold_row.get('status') if cold_row else None

  steady_rows = [r for r in rows if r['iter_int'] >= 1]
  metrics_to_compute = (
      ALL_METRICS if
      (seeks_per_iter > 0 or total_seeks_completed > 0) else BASE_METRICS)
  steady_metrics = {
      metric: summarize_metric(steady_rows, metric)
      for metric in metrics_to_compute
  }

  steady_ok_rows = [r for r in steady_rows if r.get('status') == 'ok']

  def _drift_for(col: str) -> Dict[str, Optional[float]]:
    if len(steady_ok_rows) < 200:
      return {
          'first100_n': 0,
          'first100_median_ms': None,
          'last100_start_iter': None,
          'last100_n': 0,
          'last100_median_ms': None,
          'delta_ms': None,
          'delta_pct': None,
      }
    first_rows = steady_ok_rows[:100]
    last_rows = steady_ok_rows[-100:]
    first_vals = [r[col] for r in first_rows if r.get(col) is not None]
    last_vals = [r[col] for r in last_rows if r.get(col) is not None]
    if not first_vals or not last_vals:
      return {
          'first100_n': len(first_vals),
          'first100_median_ms':
              (round(statistics.median(first_vals), 3) if first_vals else None),
          'last100_start_iter': last_rows[0]['iter_int'] if last_vals else None,
          'last100_n': len(last_vals),
          'last100_median_ms':
              (round(statistics.median(last_vals), 3) if last_vals else None),
          'delta_ms': None,
          'delta_pct': None,
      }
    raw_first = statistics.median(first_vals)
    raw_last = statistics.median(last_vals)
    raw_delta = raw_last - raw_first
    raw_pct = (100.0 * raw_delta / raw_first) if raw_first > 0 else None
    return {
        'first100_n': len(first_vals),
        'first100_median_ms': round(raw_first, 3),
        'last100_start_iter': last_rows[0]['iter_int'],
        'last100_n': len(last_vals),
        'last100_median_ms': round(raw_last, 3),
        'delta_ms': round(raw_delta, 3),
        'delta_pct': round(raw_pct, 2) if raw_pct is not None else None,
    }

  drift = {
      'create_to_presenting_ms': _drift_for('create_to_presenting_ms'),
      'destroy_ms': _drift_for('destroy_ms'),
  }
  pss = extract_periodic_pss(bundle_dir)

  setup_error = manifest.get('setup_error')
  runner_timed_out = manifest.get('runner_timed_out')
  host_exit_code = manifest.get('host_exit_code')
  runner_exit_code = manifest.get('runner_exit_code')
  manifest_csv = manifest.get('csv') or {}
  signal_interrupted = isinstance(host_exit_code, int) and host_exit_code >= 128
  ordered_iters = [r['iter_int'] for r in rows]
  is_contiguous = ordered_iters == list(range(row_count))
  last_status = (rows[-1].get('status') or '').strip() if rows else ''
  hung_stop = bool(rows and is_contiguous and last_status.endswith('_hung'))

  base_manifest_ok = (
      os.path.isdir(bundle_dir) and valid_manifest and setup_error is None and
      not signal_interrupted and
      manifest_args.get('iterations') == expected_iters and
      bool(manifest_csv.get('csv_present')))
  is_valid_hung_stop = (
      base_manifest_ok and hung_stop and 0 < row_count <= expected_iters and
      manifest_csv.get('row_count') == row_count and
      runner_exit_code is not None)
  is_full_complete = (
      base_manifest_ok and runner_timed_out is False and
      runner_exit_code is not None and
      manifest_csv.get('row_count') == expected_iters and
      row_count == expected_iters)
  is_complete = is_full_complete or is_valid_hung_stop
  bundle_status = 'complete' if is_complete else (
      'missing' if not os.path.exists(bundle_dir) else 'incomplete')

  return {
      'name': display_name,
      'bundle_dir': bundle_dir,
      'status': bundle_status,
      'setup_error': setup_error,
      'runner_timed_out': runner_timed_out,
      'host_exit_code': manifest.get('host_exit_code'),
      'runner_exit_code': manifest.get('runner_exit_code'),
      'wall_duration_sec': manifest.get('wall_duration_sec'),
      'seconds_per_iteration_wall': manifest.get('seconds_per_iteration_wall'),
      'expected_iterations': expected_iters,
      'seeks_per_iteration': seeks_per_iter,
      'requested_gap_ms': requested_gap_ms,
      'video': cell_spec.get('video') or manifest_args.get('video'),
      'color_mode': (cell_spec.get('color_mode') if 'color_mode' in cell_spec
                     else manifest_args.get('color_mode')),
      'experimental_features':
          (cell_spec.get('experimental_features') if 'experimental_features'
           in cell_spec else manifest_args.get('experimental_features')),
      'raw_csv_row_count': raw_count,
      'row_count': row_count,
      'ok_count': ok_count,
      'failed_count': failed_count,
      'stall_count': stall_count,
      'max_ok_ctp_ms':
          (round(max_ok_ctp_ms, 3) if max_ok_ctp_ms is not None else None),
      'max_ok_destroy_ms': (
          round(max_ok_destroy_ms, 3) if max_ok_destroy_ms is not None else None
      ),
      'max_ok_seek_ms':
          (round(max_ok_seek_ms, 3) if max_ok_seek_ms is not None else None),
      'total_seeks_completed': total_seeks_completed,
      'status_counts': status_counts,
      'error_breakdown': error_breakdown,
      'cold_row0': {
          'status': cold_status,
          'create_to_presenting_ms': cold_ctp_ms,
      },
      'steady_metrics': steady_metrics,
      'drift': drift,
      'package_pss': pss,
  }


def evaluate_bundle_findings(
    bundle: Dict[str, Any],
    *,
    stall_ms: float,
    pss_growth_mb: float,
    drift_ms: float,
    drift_pct: float,
) -> Dict[str, Any]:
  """Evaluates per-bundle threshold findings (failure, stall, drift, PSS)."""
  if bundle.get('status') == 'missing':
    return {
        'failure': None,
        'stall': None,
        'drift': None,
        'pss_growth': None,
        'flagged': False,
        'summary': 'missing',
    }

  flags: List[str] = []

  failed_cnt = int(bundle['failed_count'])
  row_cnt = int(bundle['row_count'])
  has_failure = failed_cnt > 0
  if has_failure:
    flags.append(f'failure({failed_cnt}/{row_cnt})')

  stall_cnt = int(bundle['stall_count'])
  has_stall = stall_cnt > 0
  if has_stall:
    flags.append(f'stall({stall_cnt} rows >= {stall_ms:g}ms)')

  drift_ctp = bundle['drift']['create_to_presenting_ms']
  drift_delta = drift_ctp['delta_ms']
  drift_delta_pct = drift_ctp['delta_pct']
  if drift_delta is None or drift_delta_pct is None:
    drift_flagged: Optional[bool] = None
  else:
    drift_flagged = (drift_delta > drift_ms) and (drift_delta_pct > drift_pct)
    if drift_flagged:
      flags.append(f'drift({drift_delta:+.2f}ms / {drift_delta_pct:+.2f}% > '
                   f'+{drift_ms:g}ms & +{drift_pct:g}%)')

  pss_delta_mb = bundle['package_pss']['delta_mb']
  if pss_delta_mb is None:
    pss_flagged: Optional[bool] = None
  else:
    pss_flagged = pss_delta_mb > pss_growth_mb
    if pss_flagged:
      flags.append(f'pss_growth({pss_delta_mb:+.2f}MB > +{pss_growth_mb:g}MB)')

  return {
      'failure': has_failure,
      'stall': has_stall,
      'drift': drift_flagged,
      'pss_growth': pss_flagged,
      'flagged': bool(flags),
      'summary': ', '.join(flags) if flags else 'none',
  }


def compute_pair_comparisons(
    cells: Sequence[Dict[str, Any]],
    analyzed_single_cells: Dict[str, Dict[str, Any]],
) -> List[Dict[str, Any]]:
  """Computes ON minus OFF steady metrics when pairs are declared."""
  pair_order: List[str] = []
  pair_arms: Dict[str, Dict[str, str]] = {}
  for cell in cells:
    pair_name = cell.get('pair')
    arm_name = cell.get('arm')
    if pair_name and arm_name:
      if pair_name not in pair_arms:
        pair_order.append(pair_name)
        pair_arms[pair_name] = {}
      pair_arms[pair_name][arm_name] = cell['name']

  def _diff_pair(
      on_val: Optional[float],
      off_val: Optional[float],
  ) -> Tuple[Optional[float], Optional[float]]:
    if on_val is None or off_val is None:
      return None, None
    delta = round(on_val - off_val, 3)
    pct = round(100.0 * delta / off_val, 2) if off_val != 0 else None
    return delta, pct

  results: List[Dict[str, Any]] = []
  for pair_name in pair_order:
    off_name = pair_arms[pair_name]['off']
    on_name = pair_arms[pair_name]['on']
    b_off = analyzed_single_cells.get(off_name)
    b_on = analyzed_single_cells.get(on_name)
    if b_off is None or b_on is None:
      continue

    off_ctp = b_off['steady_metrics']['create_to_presenting_ms']
    on_ctp = b_on['steady_metrics']['create_to_presenting_ms']
    off_dst = b_off['steady_metrics']['destroy_ms']
    on_dst = b_on['steady_metrics']['destroy_ms']
    off_cr = b_off['steady_metrics']['create_ms']
    on_cr = b_on['steady_metrics']['create_ms']
    off_sk = b_off['steady_metrics'].get('seek_median_ms') or {}
    on_sk = b_on['steady_metrics'].get('seek_median_ms') or {}

    d_ctp_med, pct_ctp_med = _diff_pair(on_ctp['median'], off_ctp['median'])
    d_ctp_p95, pct_ctp_p95 = _diff_pair(on_ctp['p95'], off_ctp['p95'])
    d_dst_med, pct_dst_med = _diff_pair(on_dst['median'], off_dst['median'])
    d_dst_p95, pct_dst_p95 = _diff_pair(on_dst['p95'], off_dst['p95'])
    d_cr_med, pct_cr_med = _diff_pair(on_cr['median'], off_cr['median'])
    d_sk_med, pct_sk_med = _diff_pair(on_sk.get('median'), off_sk.get('median'))

    off_ok_cnt = b_off['ok_count']
    off_row_cnt = b_off['row_count']
    off_fail_cnt = b_off['failed_count']
    on_ok_cnt = b_on['ok_count']
    on_row_cnt = b_on['row_count']
    on_fail_cnt = b_on['failed_count']

    results.append({
        'pair': pair_name,
        'off_cell': off_name,
        'on_cell': on_name,
        'off_status': b_off['status'],
        'on_status': b_on['status'],
        'off_ok': f'{off_ok_cnt}/{off_row_cnt}',
        'on_ok': f'{on_ok_cnt}/{on_row_cnt}',
        'off_ok_count': off_ok_cnt,
        'on_ok_count': on_ok_cnt,
        'off_failed_count': off_fail_cnt,
        'on_failed_count': on_fail_cnt,
        'create_ms_median_delta_ms': d_cr_med,
        'create_ms_median_delta_pct': pct_cr_med,
        'ctp_median_off_ms': off_ctp['median'],
        'ctp_median_on_ms': on_ctp['median'],
        'ctp_median_delta_ms': d_ctp_med,
        'ctp_median_delta_pct': pct_ctp_med,
        'ctp_p95_off_ms': off_ctp['p95'],
        'ctp_p95_on_ms': on_ctp['p95'],
        'ctp_p95_delta_ms': d_ctp_p95,
        'ctp_p95_delta_pct': pct_ctp_p95,
        'seek_median_off_ms': off_sk.get('median'),
        'seek_median_on_ms': on_sk.get('median'),
        'seek_median_delta_ms': d_sk_med,
        'seek_median_delta_pct': pct_sk_med,
        'destroy_median_off_ms': off_dst['median'],
        'destroy_median_on_ms': on_dst['median'],
        'destroy_median_delta_ms': d_dst_med,
        'destroy_median_delta_pct': pct_dst_med,
        'destroy_p95_off_ms': off_dst['p95'],
        'destroy_p95_on_ms': on_dst['p95'],
        'destroy_p95_delta_ms': d_dst_p95,
        'destroy_p95_delta_pct': pct_dst_p95,
    })

  return results


def _fmt(
    val: Optional[float],
    digits: int = 2,
    signed: bool = False,
) -> str:
  if val is None or (isinstance(val, float) and math.isnan(val)):
    return 'N/A'
  if signed:
    return f'{val:+.{digits}f}'
  return f'{val:.{digits}f}'


def _fmt_quad(metric: Dict[str, Optional[float]]) -> str:
  if not metric or metric.get('n', 0) == 0:
    return 'N/A'
  med = _fmt(metric['median'])
  p95 = _fmt(metric['p95'])
  p99 = _fmt(metric['p99'])
  max_v = _fmt(metric['max'])
  return f'{med} / {p95} / {p99} / {max_v}'


def _fmt_delta_cell(
    delta_ms: Optional[float],
    delta_pct: Optional[float],
) -> str:
  if delta_ms is None:
    return 'N/A'
  pct_part = (f' ({_fmt(delta_pct, 2, signed=True)}%)'
              if delta_pct is not None else '')
  return f'{_fmt(delta_ms, 2, signed=True)} ms{pct_part}'


def render_markdown(summary: Dict[str, Any]) -> str:
  """Renders the Markdown report from the computed summary dictionary."""
  lines: List[str] = []
  matrix_path = summary['matrix_path']
  bundles_dir = summary['bundles_dir']
  thresholds = summary['thresholds']
  stall_ms = thresholds['stall_ms']
  pss_growth_mb = thresholds['pss_growth_mb']
  drift_ms = thresholds['drift_ms']
  drift_pct = thresholds['drift_pct']

  lines.append('# SbPlayer Endurance Summary')
  lines.append('')
  lines.append(f'- **Matrix:** `{matrix_path}`')
  lines.append(f'- **Bundles directory:** `{bundles_dir}`')
  lines.append('')

  # 1. Per-Cell Table
  lines.append('## 1. Per-Cell Results')
  lines.append('')
  lines.append('> Steady-state metrics cover `status == "ok"` rows `1..N-1` '
               '(excluding cold row `0`) as `median / p95 / p99 / max` in ms.')
  lines.append('')
  lines.append(f'| Cell / Bundle | Rows (`ok/total`) & Statuses |'
               f' Stalls (`>={stall_ms:g}ms`) | Cold Row 0 `ctp_ms` |'
               ' Steady `create_ms` (`med / p95 / p99 / max`) |'
               ' Steady `create_to_presenting_ms` (`med / p95 / p99 / max`) |'
               ' Steady `destroy_ms` (`med / p95 / p99 / max`) |'
               ' Steady `gap_ms` (`med / p95 / p99 / max`) |'
               ' Drift `ctp_ms` (`1..100 --> last 100`, delta) |'
               ' Package PSS (`first --> last`, delta MB) | Wall (s) |')
  lines.append('| :--- | :--- | ---: | ---: | ---: | ---: | ---: | ---: |'
               ' ---: | ---: | ---: |')

  seek_bundles: List[Dict[str, Any]] = []
  flat_bundles: List[Dict[str, Any]] = []

  for cell_entry in summary['cells']:
    if cell_entry['status'] == 'not_triggered':
      c_name = cell_entry['name']
      lines.append(f'| `{c_name}` | `not_triggered` | N/A | N/A | N/A | N/A |'
                   ' N/A | N/A | N/A | N/A | N/A |')
      continue
    bundles = (
        cell_entry['runs']
        if cell_entry.get('repeat') is not None else [cell_entry['bundle']])
    for b in bundles:
      flat_bundles.append(b)
      if b['seeks_per_iteration'] > 0 or b['total_seeks_completed'] > 0:
        seek_bundles.append(b)

      b_name = b['name']
      b_ok = b['ok_count']
      b_rows = b['row_count']
      b_fail = b['failed_count']
      b_stalls = b['stall_count']
      if b['status'] == 'missing' and b_rows == 0:
        status_desc = '`missing`'
        stalls_str = 'N/A'
      else:
        non_ok = {
            k: v for k, v in sorted(b['status_counts'].items()) if k != 'ok'
        }
        status_tail = ('ok only' if not non_ok else ', '.join(
            f'{k}={v}' for k, v in non_ok.items()))
        status_desc = f'`{b_ok}/{b_rows}` (`fail={b_fail}`: {status_tail})'
        stalls_str = str(b_stalls)

      steady = b['steady_metrics']
      drift_ctp = b['drift']['create_to_presenting_ms']
      f100 = drift_ctp['first100_median_ms']
      l100 = drift_ctp['last100_median_ms']
      d_drift = drift_ctp['delta_ms']
      d_pct = drift_ctp['delta_pct']
      if f100 is not None and l100 is not None:
        pct_str = (f', {_fmt(d_pct, 2, signed=True)}%'
                   if d_pct is not None else '')
        drift_str = (f'{_fmt(f100)} --> {_fmt(l100)} '
                     f'({_fmt(d_drift, 2, signed=True)}{pct_str})')
      else:
        drift_str = 'N/A'

      pss = b['package_pss']
      if pss['start_mb'] is not None and pss['end_mb'] is not None:
        p_start = _fmt(pss['start_mb'])
        p_end = _fmt(pss['end_mb'])
        p_s_it = pss['start_iter']
        p_e_it = pss['end_iter']
        p_delta = _fmt(pss['delta_mb'], 2, signed=True)
        pss_str = (
            f'{p_start} --> {p_end} MB (`@{p_s_it}-->@{p_e_it}`, {p_delta})')
      else:
        pss_str = 'N/A'

      cold_ctp = _fmt(b['cold_row0']['create_to_presenting_ms'])
      cr_str = _fmt_quad(steady['create_ms'])
      ctp_str = _fmt_quad(steady['create_to_presenting_ms'])
      dst_str = _fmt_quad(steady['destroy_ms'])
      gap_str = _fmt_quad(steady['gap_ms'])
      wall_str = _fmt(b['wall_duration_sec'], 1)
      lines.append(f'| `{b_name}` | {status_desc} | {stalls_str} | {cold_ctp} |'
                   f' {cr_str} | {ctp_str} | {dst_str} | {gap_str} |'
                   f' {drift_str} | {pss_str} | {wall_str} |')

  if seek_bundles:
    lines.append('')
    lines.append('### Seek Metrics (`seeks > 0`)')
    lines.append('')
    lines.append('| Cell / Bundle | Rows (`ok/total`) | Total Seeks Completed |'
                 ' Steady `seek_median_ms` (`med / p95 / p99 / max`) |'
                 ' Steady `seek_max_ms` (`med / p95 / p99 / max`) |')
    lines.append('| :--- | ---: | ---: | ---: | ---: |')
    for b in seek_bundles:
      steady = b['steady_metrics']
      b_name = b['name']
      b_ok = b['ok_count']
      b_rows = b['row_count']
      b_seeks = b['total_seeks_completed']
      sm_str = _fmt_quad(steady.get('seek_median_ms', {}))
      sx_str = _fmt_quad(steady.get('seek_max_ms', {}))
      lines.append(f'| `{b_name}` | `{b_ok}/{b_rows}` | {b_seeks} |'
                   f' {sm_str} | {sx_str} |')

  # 2. Per-Cell Findings Section
  lines.append('')
  lines.append('## 2. Per-Cell Findings')
  lines.append('')
  lines.append('- **Thresholds:**')
  lines.append('  - **Failure:** any non-`ok` iteration (`failed_count > 0`)')
  lines.append(
      f'  - **Stall:** any `ok` iteration with `create_to_presenting_ms >= '
      f'{stall_ms:g} ms`, `seek_max_ms >= {stall_ms:g} ms`, or `destroy_ms >= '
      f'{stall_ms:g} ms`')
  lines.append(
      f'  - **Drift:** steady `create_to_presenting_ms` `last-100 median - '
      f'first-100 median` `> +{drift_ms:g} ms` and `> +{drift_pct:g}%`')
  lines.append(
      f'  - **Memory growth (`pss_growth`):** periodic package PSS delta '
      f'`> +{pss_growth_mb:g} MB`')
  lines.append('')

  flagged_bundles = [
      b for b in flat_bundles if b.get('findings', {}).get('flagged')
  ]
  if not flagged_bundles:
    completed_cnt = sum(1 for b in flat_bundles if b['status'] == 'complete')
    lines.append(
        f'No threshold findings flagged across {completed_cnt} completed '
        'bundle(s).')
  else:
    lines.append('| Cell / Bundle | Failure | Stall | Drift |'
                 ' Memory Growth (PSS) | Summary |')
    lines.append('| :--- | :--- | :--- | :--- | :--- | :--- |')
    for b in flagged_bundles:
      b_name = b['name']
      b_status = b['status']
      b_label = (f'`{b_name}`'
                 if b_status == 'complete' else f'`{b_name}` ({b_status})')
      fnd = b['findings']
      f_fail = 'YES' if fnd['failure'] else 'no'
      f_stall = 'YES' if fnd['stall'] else 'no'
      if fnd['drift'] is None:
        f_drift = 'N/A'
      else:
        f_drift = 'YES' if fnd['drift'] else 'no'
      if fnd['pss_growth'] is None:
        f_pss = 'N/A'
      else:
        f_pss = 'YES' if fnd['pss_growth'] else 'no'
      f_sum = fnd['summary']
      lines.append(f'| {b_label} | {f_fail} | {f_stall} | {f_drift} |'
                   f' {f_pss} | {f_sum} |')

  # 3. Optional Paired ON minus OFF Comparisons (only if pairs exist)
  if summary['pairs']:
    lines.append('')
    lines.append('## 3. Paired `ON - OFF` Comparisons')
    lines.append('')
    lines.append('| Pair | `ok` (`OFF` / `ON`) |'
                 ' `create_to_presenting_ms` Med `ON - OFF` |'
                 ' `create_to_presenting_ms` p95 `ON - OFF` |'
                 ' `seek_median_ms` Med `ON - OFF` |'
                 ' `destroy_ms` Med `ON - OFF` |'
                 ' `destroy_ms` p95 `ON - OFF` |')
    lines.append('| :--- | :--- | ---: | ---: | ---: | ---: | ---: |')
    for pair in summary['pairs']:
      p_name = pair['pair']
      off_ok = pair['off_ok']
      on_ok = pair['on_ok']
      ctp_med_d = _fmt_delta_cell(pair['ctp_median_delta_ms'],
                                  pair['ctp_median_delta_pct'])
      ctp_p95_d = _fmt_delta_cell(pair['ctp_p95_delta_ms'],
                                  pair['ctp_p95_delta_pct'])
      sk_med_d = _fmt_delta_cell(
          pair.get('seek_median_delta_ms'), pair.get('seek_median_delta_pct'))
      dst_med_d = _fmt_delta_cell(pair['destroy_median_delta_ms'],
                                  pair['destroy_median_delta_pct'])
      dst_p95_d = _fmt_delta_cell(pair['destroy_p95_delta_ms'],
                                  pair['destroy_p95_delta_pct'])
      lines.append(f'| `{p_name}` | `{off_ok}` / `{on_ok}` |'
                   f' {ctp_med_d} | {ctp_p95_d} | {sk_med_d} |'
                   f' {dst_med_d} | {dst_p95_d} |')

  lines.append('')
  return '\n'.join(lines)


def build_summary(
    matrix_path: str,
    bundles_dir: str,
    *,
    stall_ms: float = DEFAULT_STALL_MS,
    pss_growth_mb: float = DEFAULT_PSS_GROWTH_MB,
    drift_ms: float = DEFAULT_DRIFT_MS,
    drift_pct: float = DEFAULT_DRIFT_PCT,
) -> Dict[str, Any]:
  """Builds the summary dict for `--out-json` and `--out-md`."""
  with open(matrix_path, 'r', encoding='utf-8') as matrix_file:
    raw_matrix = json.load(matrix_file)
  cells = validate_matrix(raw_matrix)

  state_cells: Dict[str, Any] = {}
  state_path = os.path.join(bundles_dir, 'matrix_state.json')
  if os.path.isfile(state_path):
    try:
      with open(state_path, 'r', encoding='utf-8') as state_file:
        state_data = json.load(state_file)
      if isinstance(state_data, dict) and isinstance(
          state_data.get('cells'), dict):
        state_cells = state_data['cells']
    except (OSError, ValueError, json.JSONDecodeError):
      state_cells = {}

  cell_entries: List[Dict[str, Any]] = []
  analyzed_single_cells: Dict[str, Dict[str, Any]] = {}
  failed_counts_by_cell: Dict[str, int] = {}
  stall_counts_by_cell: Dict[str, int] = {}
  status_by_cell: Dict[str, str] = {}

  for cell in cells:
    name = cell['name']
    repeat_count = cell.get('repeat')
    refs_failed = cell.get('run_if_any_failed') or []
    refs_stalled = cell.get('run_if_any_stalled') or []
    recorded_status = (state_cells.get(name) or {}).get('status')

    if repeat_count is None:
      has_bundle_on_disk = os.path.exists(os.path.join(bundles_dir, name))
    else:
      has_bundle_on_disk = any(
          os.path.exists(os.path.join(bundles_dir, name, f'run_{idx}'))
          for idx in range(1,
                           int(repeat_count) + 1))

    if not has_bundle_on_disk and (refs_failed or refs_stalled):
      all_refs = list(refs_failed) + [
          r for r in refs_stalled if r not in refs_failed
      ]
      prereqs_done = all(
          status_by_cell.get(ref) in ('complete', 'not_triggered')
          for ref in all_refs)
      no_fail_trigger = all(
          failed_counts_by_cell.get(ref, 0) == 0 for ref in refs_failed)
      no_stall_trigger = all(
          stall_counts_by_cell.get(ref, 0) == 0 for ref in refs_stalled)
      if recorded_status == 'not_triggered' or (recorded_status is None and
                                                prereqs_done and
                                                no_fail_trigger and
                                                no_stall_trigger):
        status_by_cell[name] = 'not_triggered'
        failed_counts_by_cell[name] = 0
        stall_counts_by_cell[name] = 0
        not_triggered_entry: Dict[str, Any] = {
            'name': name,
            'status': 'not_triggered',
            'repeat': int(repeat_count) if repeat_count is not None else None,
            'pair': cell.get('pair'),
            'arm': cell.get('arm'),
            'run_if_any_failed': refs_failed or None,
            'run_if_any_stalled': refs_stalled or None,
        }
        if repeat_count is None:
          not_triggered_entry['bundle'] = None
        else:
          not_triggered_entry['runs'] = []
        cell_entries.append(not_triggered_entry)
        continue

    if repeat_count is None:
      bundle_dir = os.path.join(bundles_dir, name)
      bundle_summary = analyze_bundle(bundle_dir, name, cell, stall_ms=stall_ms)
      analyzed_single_cells[name] = bundle_summary
      status_by_cell[name] = bundle_summary['status']
      failed_counts_by_cell[name] = int(bundle_summary['failed_count'])
      stall_counts_by_cell[name] = int(bundle_summary['stall_count'])
      cell_entries.append({
          'name': name,
          'status': bundle_summary['status'],
          'repeat': None,
          'pair': cell.get('pair'),
          'arm': cell.get('arm'),
          'run_if_any_failed': refs_failed or None,
          'run_if_any_stalled': refs_stalled or None,
          'bundle': bundle_summary,
      })
    else:
      run_summaries: List[Dict[str, Any]] = []
      for run_idx in range(1, int(repeat_count) + 1):
        run_dir = os.path.join(bundles_dir, name, f'run_{run_idx}')
        run_summaries.append(
            analyze_bundle(
                run_dir, f'{name}/run_{run_idx}', cell, stall_ms=stall_ms))

      all_complete = all(r['status'] == 'complete' for r in run_summaries)
      total_failed = sum(int(r['failed_count']) for r in run_summaries)
      total_stalls = sum(int(r['stall_count']) for r in run_summaries)
      total_ok = sum(int(r['ok_count']) for r in run_summaries)
      total_rows = sum(int(r['row_count']) for r in run_summaries)
      total_wall = sum(
          float(r['wall_duration_sec'] or 0.0) for r in run_summaries)
      merged_status_counts: Dict[str, int] = {}
      for r in run_summaries:
        for status_key, count in r['status_counts'].items():
          merged_status_counts[status_key] = (
              merged_status_counts.get(status_key, 0) + int(count))

      agg_status = 'complete' if all_complete else 'incomplete'
      status_by_cell[name] = agg_status
      failed_counts_by_cell[name] = total_failed
      stall_counts_by_cell[name] = total_stalls
      cell_entries.append({
          'name': name,
          'status': agg_status,
          'repeat': int(repeat_count),
          'pair': cell.get('pair'),
          'arm': cell.get('arm'),
          'run_if_any_failed': refs_failed or None,
          'run_if_any_stalled': refs_stalled or None,
          'row_count': total_rows,
          'ok_count': total_ok,
          'failed_count': total_failed,
          'stall_count': total_stalls,
          'status_counts': merged_status_counts,
          'wall_duration_sec': round(total_wall, 3),
          'runs': run_summaries,
      })

  flagged_cells_summary: List[Dict[str, Any]] = []
  for entry in cell_entries:
    if entry['status'] == 'not_triggered':
      continue
    bundles = (
        entry['runs'] if entry.get('repeat') is not None else [entry['bundle']])
    for b in bundles:
      b['findings'] = evaluate_bundle_findings(
          b,
          stall_ms=stall_ms,
          pss_growth_mb=pss_growth_mb,
          drift_ms=drift_ms,
          drift_pct=drift_pct,
      )
      if b['findings']['flagged']:
        flagged_cells_summary.append({
            'bundle': b['name'],
            'findings': b['findings'],
        })

  pairs = compute_pair_comparisons(cells, analyzed_single_cells)

  return {
      'matrix_path': os.path.abspath(matrix_path),
      'bundles_dir': os.path.abspath(bundles_dir),
      'thresholds': {
          'stall_ms': stall_ms,
          'pss_growth_mb': pss_growth_mb,
          'drift_ms': drift_ms,
          'drift_pct': drift_pct,
      },
      'findings': {
          'flagged_bundle_count': len(flagged_cells_summary),
          'flagged_bundles': flagged_cells_summary,
      },
      'cells': cell_entries,
      'pairs': pairs,
  }


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description=(
          'Summarize SbPlayer endurance bundles into JSON and Markdown.'))
  parser.add_argument(
      '--matrix', required=True, help='Path to the matrix JSON file.')
  parser.add_argument(
      '--bundles-dir',
      required=True,
      help='Directory containing cell bundle directories.')
  parser.add_argument(
      '--out-json',
      required=True,
      help='Destination path for the JSON summary output.')
  parser.add_argument(
      '--out-md',
      required=True,
      help='Destination path for the Markdown summary report.')
  parser.add_argument(
      '--stall-ms',
      type=float,
      default=DEFAULT_STALL_MS,
      help=('Threshold in ms on ok rows (create_to_presenting_ms, '
            'seek_max_ms, or destroy_ms) to flag a stall (default: 1500).'))
  parser.add_argument(
      '--pss-growth-mb',
      type=float,
      default=DEFAULT_PSS_GROWTH_MB,
      help=('Threshold in MB on periodic package PSS delta to flag memory '
            'growth (default: 20).'))
  parser.add_argument(
      '--drift-ms',
      type=float,
      default=DEFAULT_DRIFT_MS,
      help=('Minimum positive shift in ms on steady create_to_presenting_ms '
            'last-100 minus first-100 median to flag drift (default: 50).'))
  parser.add_argument(
      '--drift-pct',
      type=float,
      default=DEFAULT_DRIFT_PCT,
      help=('Minimum positive percentage increase on steady '
            'create_to_presenting_ms last-100 vs first-100 median to flag '
            'drift (default: 25).'))
  args = parser.parse_args(argv)
  if not math.isfinite(args.stall_ms) or args.stall_ms <= 0:
    parser.error('--stall-ms must be finite and > 0')
  if not math.isfinite(args.pss_growth_mb) or args.pss_growth_mb <= 0:
    parser.error('--pss-growth-mb must be finite and > 0')
  if not math.isfinite(args.drift_ms) or args.drift_ms <= 0:
    parser.error('--drift-ms must be finite and > 0')
  if not math.isfinite(args.drift_pct) or args.drift_pct <= 0:
    parser.error('--drift-pct must be finite and > 0')
  return args


def main(argv: Optional[Sequence[str]] = None) -> int:
  args = _parse_args(argv if argv is not None else sys.argv[1:])
  try:
    summary = build_summary(
        args.matrix,
        args.bundles_dir,
        stall_ms=args.stall_ms,
        pss_growth_mb=args.pss_growth_mb,
        drift_ms=args.drift_ms,
        drift_pct=args.drift_pct,
    )
  except (OSError, ValueError, json.JSONDecodeError) as exc:
    print(f'[summarize] Error: {exc}', file=sys.stderr)
    return 2

  markdown = render_markdown(summary)

  out_json_dir = os.path.dirname(os.path.abspath(args.out_json))
  if out_json_dir:
    os.makedirs(out_json_dir, exist_ok=True)
  with open(args.out_json, 'w', encoding='utf-8') as json_file:
    json.dump(summary, json_file, indent=2, sort_keys=False, allow_nan=False)
    json_file.write('\n')

  out_md_dir = os.path.dirname(os.path.abspath(args.out_md))
  if out_md_dir:
    os.makedirs(out_md_dir, exist_ok=True)
  with open(args.out_md, 'w', encoding='utf-8') as md_file:
    md_file.write(markdown)

  return 0


if __name__ == '__main__':
  sys.exit(main())

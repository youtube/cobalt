#!/usr/bin/env python3
#
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
"""Reads, writes and subtracts pull_uma_histogram_set_via_cdp.py dump files.

A dump file has one line per Browser.getHistograms query:

  <YYYY-mm-dd HH:MM:SS>,<query>,<JSON CDP response>

where the response is {"id": N, "result": {"histograms": [{"name": ...,
"sum": ..., "count": ..., "buckets": [{"low": ..., "high": ..., "count": ...}]
}]}}. Browser.getHistograms returns every histogram whose name contains the
query, so one histogram can appear under several queries.

Annotation lines (landing gate result and lifecycle exit flush status) use a
reserved query name that starts with "__" and carry
{"result": {"<query>": {...}}}. Tools that only read result.histograms skip
them.

Pure Python (no third-party imports), so android_ab_report.py runs with python3.
"""

import json
import time

LANDING_GATE_KEY = '__landing_gate__'
EXIT_FLUSH_KEY = '__exit_flush__'

# Web Page Replay (WPR) defaults for deterministic replay.
DEFAULT_WPR_HTTP_PORT = 8080
DEFAULT_WPR_HTTPS_PORT = 8081
DEFAULT_WPR_HOST_RESOLVER_RULES = f'MAP * 127.0.0.1:{DEFAULT_WPR_HTTPS_PORT}'
DEFAULT_WPR_SPKI_LIST = 'PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I='
# Kabuki URL params passed through Cobalt's url_params intent extra:
# force_startup_screen=none skips the "Who's watching?" account picker and the
# welcome screens; hl and gl pin the UI language and the content region.
DEFAULT_URL_PARAMS = 'force_startup_screen=none,hl=en,gl=US'
# Exit code of pull_uma_histogram_set_via_cdp.py when the landing gate fails.
LANDING_GATE_EXIT_CODE = 3


def timestamp() -> str:
  return time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())


def format_line(query: str, payload: dict, ts: str = None) -> str:
  """Formats one dump line."""
  return f'{ts or timestamp()},{query},{json.dumps(payload)}\n'


def write_annotation(output_file, key: str, data: dict):
  """Appends an annotation line (ignored by histogram parsers)."""
  if output_file:
    output_file.write(format_line(key, {'result': {key: data}}))
    output_file.flush()


def matches_query(name: str, query: str) -> bool:
  """Returns True if histogram `name` matches `query` on a dot boundary.

  CDP `Browser.getHistograms` performs substring matching, so querying
  `Cobalt.Storage.Cookie.CommitDuration` also returns
  `Cobalt.Storage.Cookie.CommitDurationPerOperation`. Matching on dot
  boundaries keeps exact matches and dot-suffixed sub-histograms (e.g.,
  `HttpCache.Pattern.*`, `V8.CompileScriptMicroSeconds.*`) while rejecting
  unrelated suffix extensions.
  """
  if not query:
    return True
  if query.endswith('.'):
    return name.startswith(query)
  return name == query or name.startswith(query + '.')


def parse_dump_lines(lines):
  """Parses dump lines.

  Returns:
    (histograms, annotations): {name: histogram dict} and {key: data}. When a
    histogram appears more than once (overlapping queries or several polls),
    the last occurrence wins, which is the latest cumulative value.
  """
  histograms = {}
  annotations = {}
  for line in lines:
    parts = line.rstrip('\r\n').split(',', 2)
    if len(parts) < 3:
      continue
    try:
      payload = json.loads(parts[2])
    except json.JSONDecodeError:
      continue
    if not isinstance(payload, dict):
      continue
    result = payload.get('result') or {}
    if parts[1].startswith('__'):
      if parts[1] in result:
        annotations[parts[1]] = result[parts[1]]
      continue
    for h in result.get('histograms') or []:
      if (isinstance(h, dict) and 'name' in h and
          matches_query(h['name'], parts[1])):
        histograms[h['name']] = h
  return histograms, annotations


def parse_dump(path: str):
  """Parses a dump file. See parse_dump_lines()."""
  with open(path, encoding='utf-8') as f:
    return parse_dump_lines(f)


def histogram_delta(h0, h1) -> dict:
  """Returns the samples recorded between cumulative snapshots h0 and h1.

  h0 may be None (histogram did not exist at the first snapshot).
  """
  before = {}
  if h0:
    for b in h0.get('buckets', []):
      before[(b['low'], b['high'])] = b['count']
  buckets = []
  for b in h1.get('buckets', []):
    d = b['count'] - before.get((b['low'], b['high']), 0)
    if d:
      buckets.append({'low': b['low'], 'high': b['high'], 'count': d})
  return {
      'name': h1['name'],
      'sum': h1.get('sum', 0) - (h0.get('sum', 0) if h0 else 0),
      'count': h1.get('count', 0) - (h0.get('count', 0) if h0 else 0),
      'buckets': buckets,
  }


def snapshot_delta(s0: dict, s1: dict) -> dict:
  """Returns {name: s1 - s0} for every histogram whose count changed.

  Both snapshots are {name: cumulative histogram}. Subtracting locally (instead
  of Browser.getHistograms delta=true) leaves UMA's own SnapshotDelta() state
  untouched, so the regular UMA upload is not affected. A negative count means
  the histogram was reset between snapshots; it is kept so it stays visible.
  """
  out = {}
  for name, h1 in s1.items():
    d = histogram_delta(s0.get(name), h1)
    if d['count']:
      out[name] = d
  return out


def histograms_to_lines(histograms: dict, ts: str = None) -> list:
  """Formats {name: histogram} as dump lines (one line per histogram)."""
  ts = ts or timestamp()
  return [
      format_line(name, {
          'id': 0,
          'result': {
              'histograms': [histograms[name]]
          }
      }, ts) for name in sorted(histograms)
  ]

#!/usr/bin/env vpython3
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
"""Helper utility to download official Chromedriver with GCS caching."""

import argparse
import datetime
import json
import os
import shutil
import socket
import subprocess
import sys
import urllib.error
import urllib.request
import zipfile

_HTTP_TIMEOUT_SECONDS = 30
_DEFAULT_GCS_MAX_AGE_DAYS = 7
_SECONDS_PER_DAY = 86400
_orig_getaddrinfo = socket.getaddrinfo


def _ipv4_first_getaddrinfo(*args, **kwargs):
  res = _orig_getaddrinfo(*args, **kwargs)
  ipv4 = [r for r in res if r[0] == socket.AF_INET]
  return ipv4 if ipv4 else res


socket.getaddrinfo = _ipv4_first_getaddrinfo


def _get_default_gcs_uri():
  if not shutil.which('gcloud'):
    return None
  res = subprocess.run(
      ['gcloud', 'config', 'get-value', 'project'],
      capture_output=True,
      text=True,
      check=False,
  )
  project = res.stdout.strip()
  if res.returncode != 0 or not project or project == '(unset)':
    return None
  return f'gs://{project}-test-artifacts/chromedriver/chromedriver'


def _parse_gcs_timestamp(timestamp_str):
  normalized = timestamp_str.strip().replace('Z', '+00:00')
  dt = datetime.datetime.fromisoformat(normalized)
  if dt.tzinfo is None:
    return dt.replace(tzinfo=datetime.timezone.utc)
  return dt


def _try_download_from_gcs(gcs_uri, dest_dir, max_age_days):
  if not gcs_uri:
    return None

  print(f'Checking GCS for cached Chromedriver at {gcs_uri}...', flush=True)
  desc_res = subprocess.run(
      ['gcloud', 'storage', 'objects', 'describe', gcs_uri, '--format=json'],
      capture_output=True,
      text=True,
      check=False,
  )
  if desc_res.returncode != 0:
    print(
        f'No cached Chromedriver found at {gcs_uri}; '
        'will download from external URL.',
        flush=True)
    return None

  try:
    metadata = json.loads(desc_res.stdout)
    update_time_str = (
        metadata.get('update_time') or metadata.get('creation_time'))
    if update_time_str:
      updated_at = _parse_gcs_timestamp(update_time_str)
      now_utc = datetime.datetime.now(datetime.timezone.utc)
      age_seconds = (now_utc - updated_at).total_seconds()
      age_days = age_seconds / _SECONDS_PER_DAY
      print(
          f'Found GCS Chromedriver (last updated: {update_time_str}, '
          f'age: {age_days:.2f} days).',
          flush=True)
      if age_seconds >= max_age_days * _SECONDS_PER_DAY:
        print(
            f'GCS Chromedriver is older than {max_age_days} days; '
            'forcing fresh download from external URL.',
            flush=True)
        return None
  except (ValueError, KeyError, TypeError) as e:
    print(
        f'Warning: Could not parse GCS metadata timestamp ({e}); '
        'forcing fresh download from external URL.',
        flush=True)
    return None

  bin_path = os.path.join(dest_dir, 'chromedriver')
  cp_res = subprocess.run(
      ['gcloud', 'storage', 'cp', gcs_uri, bin_path],
      capture_output=True,
      text=True,
      check=False,
  )
  if cp_res.returncode != 0 or not os.path.exists(bin_path):
    print(
        f'Failed to download Chromedriver from {gcs_uri}: '
        f'{cp_res.stderr.strip()}',
        flush=True)
    return None

  os.chmod(bin_path, 0o755)
  print(f'Downloaded Chromedriver from GCS to: {bin_path}', flush=True)
  return bin_path


def _upload_to_gcs(bin_path, gcs_uri):
  if not gcs_uri:
    return

  print(f'Uploading Chromedriver to GCS at {gcs_uri}...', flush=True)
  up_res = subprocess.run(
      ['gcloud', 'storage', 'cp', bin_path, gcs_uri],
      capture_output=True,
      text=True,
      check=False,
  )
  if up_res.returncode == 0:
    print(f'Successfully uploaded Chromedriver to {gcs_uri}', flush=True)
  else:
    print(
        f'Warning: Failed to upload Chromedriver to {gcs_uri}: '
        f'{up_res.stderr.strip()}',
        flush=True)


def _download_file(url, dest_path):
  with urllib.request.urlopen(url, timeout=_HTTP_TIMEOUT_SECONDS) as resp:
    with open(dest_path, 'wb') as out_file:
      shutil.copyfileobj(resp, out_file)


def _resolve_closest_chromedriver_url(parts, major):
  prefixes = []
  if len(parts) >= 3:
    prefixes.append(f'{parts[0]}.{parts[1]}.{parts[2]}')
  prefixes.append(str(major))

  for prefix in prefixes:
    latest_url = ('https://googlechromelabs.github.io/chrome-for-testing/'
                  f'LATEST_RELEASE_{prefix}')
    try:
      with urllib.request.urlopen(
          latest_url, timeout=_HTTP_TIMEOUT_SECONDS) as req:
        resolved_version = req.read().decode('utf-8').strip()
      if resolved_version:
        return ('https://storage.googleapis.com/chrome-for-testing-public/'
                f'{resolved_version}/linux64/chromedriver-linux64.zip')
    except urllib.error.URLError as e:
      print(f'Failed to resolve {latest_url}: {e}', flush=True)

  api_url = ('https://googlechromelabs.github.io/chrome-for-testing/'
             'known-good-versions-with-downloads.json')
  with urllib.request.urlopen(api_url, timeout=_HTTP_TIMEOUT_SECONDS) as req:
    data = json.loads(req.read().decode('utf-8'))
  resolved_url = None
  for v in data['versions']:
    if v['version'].startswith(f'{major}.'):
      if 'chromedriver' in v['downloads']:
        for d in v['downloads']['chromedriver']:
          if d['platform'] == 'linux64':
            resolved_url = d['url']
  return resolved_url


def _download_from_external(version, dest_dir):
  try:
    parts = version.split('.')
    major = int(parts[0])
  except ValueError:
    print(f'Error parsing version: {version}', flush=True)
    sys.exit(1)

  if major < 115:
    url = (f'https://chromedriver.storage.googleapis.com/{version}/'
           'chromedriver_linux64.zip')
  else:
    url = ('https://storage.googleapis.com/chrome-for-testing-public/'
           f'{version}/linux64/chromedriver-linux64.zip')

  zip_path = os.path.join(dest_dir, 'chromedriver.zip')
  print(f'Downloading from {url}...', flush=True)
  try:
    _download_file(url, zip_path)
  except urllib.error.URLError as e:
    print(f'Failed to download: {e}', flush=True)
    if major >= 115:
      print('Falling back to resolving latest patch version...', flush=True)
      url = _resolve_closest_chromedriver_url(parts, major)
      if not url:
        raise RuntimeError(
            f'Could not resolve Chromedriver download URL for {version}') from e
      print(f'Resolved to {url}', flush=True)
      _download_file(url, zip_path)
    else:
      raise

  with zipfile.ZipFile(zip_path, 'r') as zip_ref:
    zip_ref.extractall(dest_dir)

  os.remove(zip_path)

  for root, _, files in os.walk(dest_dir):
    if 'chromedriver' in files:
      bin_path = os.path.join(root, 'chromedriver')
      os.chmod(bin_path, 0o755)
      print(f'Chromedriver downloaded and extracted to: {bin_path}', flush=True)
      return bin_path
  return None


def download_chromedriver(version,
                          dest_dir,
                          gcs_uri=None,
                          max_age_days=_DEFAULT_GCS_MAX_AGE_DAYS):
  cached_bin = _try_download_from_gcs(gcs_uri, dest_dir, max_age_days)
  if cached_bin:
    return cached_bin

  bin_path = _download_from_external(version, dest_dir)
  if bin_path and gcs_uri:
    _upload_to_gcs(bin_path, gcs_uri)
  return bin_path


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'version', help='Chromedriver version to download (e.g. 115.0.5790.170)')
  parser.add_argument(
      '--dest',
      default='/tmp/chromedriver_download',
      help='Destination directory')
  parser.add_argument(
      '--gcs-uri',
      default=None,
      help='Static GCS object URI for caching the Chromedriver binary.')
  parser.add_argument(
      '--max-age-days',
      type=float,
      default=_DEFAULT_GCS_MAX_AGE_DAYS,
      help='Max age in days for the GCS cached binary before refreshing.')
  args = parser.parse_args()

  os.makedirs(args.dest, exist_ok=True)
  gcs_uri = args.gcs_uri if args.gcs_uri is not None else _get_default_gcs_uri()
  download_chromedriver(
      args.version,
      args.dest,
      gcs_uri=gcs_uri,
      max_age_days=args.max_age_days,
  )


if __name__ == '__main__':
  main()

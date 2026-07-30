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
import urllib.request
import zipfile

_DEFAULT_CHROMEDRIVER_VERSION = '138.0.7204.183'
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


def _cached_version_matches(bin_path, expected_version):
  res = subprocess.run(
      [bin_path, '--version'],
      capture_output=True,
      text=True,
      check=False,
  )
  if res.returncode != 0:
    return False
  return expected_version in res.stdout


def _try_download_from_gcs(gcs_uri, dest_dir, expected_version, max_age_days):
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
  if not _cached_version_matches(bin_path, expected_version):
    print(
        f'GCS Chromedriver does not match expected version {expected_version}; '
        'forcing fresh download from external URL.',
        flush=True)
    os.remove(bin_path)
    return None

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


def _download_from_external(version, dest_dir):
  url = ('https://storage.googleapis.com/chrome-for-testing-public/'
         f'{version}/linux64/chromedriver-linux64.zip')
  zip_path = os.path.join(dest_dir, 'chromedriver.zip')
  print(f'Downloading Chromedriver {version} from {url}...', flush=True)
  with urllib.request.urlopen(url, timeout=_HTTP_TIMEOUT_SECONDS) as resp:
    with open(zip_path, 'wb') as out_file:
      shutil.copyfileobj(resp, out_file)

  bin_path = os.path.join(dest_dir, 'chromedriver')
  with zipfile.ZipFile(zip_path, 'r') as zip_ref:
    with zip_ref.open('chromedriver-linux64/chromedriver') as src_file:
      with open(bin_path, 'wb') as dst_file:
        shutil.copyfileobj(src_file, dst_file)

  os.remove(zip_path)
  os.chmod(bin_path, 0o755)
  print(f'Chromedriver downloaded and extracted to: {bin_path}', flush=True)
  return bin_path


def download_chromedriver(version=_DEFAULT_CHROMEDRIVER_VERSION,
                          dest_dir='/tmp/chromedriver_download',
                          gcs_uri=None,
                          max_age_days=_DEFAULT_GCS_MAX_AGE_DAYS):
  cached_bin = _try_download_from_gcs(gcs_uri, dest_dir, version, max_age_days)
  if cached_bin:
    return cached_bin

  bin_path = _download_from_external(version, dest_dir)
  if bin_path and gcs_uri:
    _upload_to_gcs(bin_path, gcs_uri)
  return bin_path


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'version',
      nargs='?',
      default=_DEFAULT_CHROMEDRIVER_VERSION,
      help=('Chromedriver version to download '
            f'(default: {_DEFAULT_CHROMEDRIVER_VERSION})'))
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
      version=args.version,
      dest_dir=args.dest,
      gcs_uri=gcs_uri,
      max_age_days=args.max_age_days,
  )


if __name__ == '__main__':
  main()

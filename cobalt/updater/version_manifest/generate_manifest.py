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
"""Generates the Evergreen manifest.json for a Cobalt installation.

The Evergreen version is derived from the Cobalt version using the scheme
documented at go/cobalt-evergreen-versioning:

  Cobalt <MAJOR>.lts.<MINOR>  ->  Evergreen <MAJOR - 20>.<MINOR>.<RC>

where <RC> is the release candidate type and 2 denotes the "latest" package.
For example, Cobalt 27.lts.10 maps to Evergreen 7.10.2.

Non-LTS versions (e.g. 27.master.0) have no Evergreen counterpart, so the
manifest is written with the default Evergreen version 1.0.0 instead. This is
the version the loader app and the updater assume when no manifest is present,
so development builds keep their existing behavior.
"""

import argparse
import json
import re
import sys

_LTS_VERSION_PATTERN = re.compile(r'^(\d+)\.lts\.(\d+)$')

# Cobalt major version N maps to Evergreen major version N - 20.
_EVERGREEN_MAJOR_VERSION_OFFSET = 20

# Release candidate type of the "latest" Evergreen package.
_LATEST_RELEASE_CANDIDATE = 2

# The Evergreen version used when the Cobalt version has no Evergreen
# counterpart. Must match |kDefaultManifestVersion| in cobalt/updater/util.cc.
DEFAULT_EVERGREEN_VERSION = '1.0.0'


def is_lts_version(cobalt_version):
  """Returns True if |cobalt_version| is an LTS version, e.g. '27.lts.10'."""
  return _LTS_VERSION_PATTERN.match(cobalt_version) is not None


def cobalt_to_evergreen_version(cobalt_version):
  """Converts a Cobalt version string to an Evergreen version string.

  Args:
    cobalt_version: The Cobalt version, e.g. '27.lts.10' or '27.master.0'.

  Returns:
    The Evergreen version, e.g. '7.10.2' for '27.lts.10', or
    DEFAULT_EVERGREEN_VERSION for non-LTS versions such as '27.master.0'.

  Raises:
    ValueError: If |cobalt_version| is an LTS version that predates Evergreen
      versioning.
  """
  match = _LTS_VERSION_PATTERN.match(cobalt_version)
  if not match:
    return DEFAULT_EVERGREEN_VERSION
  major = int(match.group(1)) - _EVERGREEN_MAJOR_VERSION_OFFSET
  if major < 1:
    raise ValueError(
        f'Cobalt version {cobalt_version!r} predates Evergreen versioning')
  minor = int(match.group(2))
  return f'{major}.{minor}.{_LATEST_RELEASE_CANDIDATE}'


def build_manifest(evergreen_version):
  """Returns the manifest dictionary for |evergreen_version|."""
  return {
      'manifest_version': 2,
      'name': 'Cobalt',
      'description': 'Cobalt',
      'version': evergreen_version,
  }


def write_manifest(cobalt_version, output_path):
  """Writes the Evergreen manifest for |cobalt_version| to |output_path|."""
  manifest = build_manifest(cobalt_to_evergreen_version(cobalt_version))
  with open(output_path, 'w', encoding='utf-8') as f:
    json.dump(manifest, f, indent=4)
    f.write('\n')


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--cobalt-version',
      required=True,
      help='The Cobalt version, e.g. 27.lts.10 or 27.master.0.')
  parser.add_argument(
      '--output', required=True, help='Path of the manifest.json to write.')
  args = parser.parse_args(argv)

  try:
    write_manifest(args.cobalt_version, args.output)
  except ValueError as e:
    print(f'generate_manifest.py: {e}', file=sys.stderr)
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

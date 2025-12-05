#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
#
"""Packages Cobalt with a given layout.

The JSON format that specifies the package layout comes from Chromium's
https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/archive/properties.proto
definition though Cobalt only implements a subset of the archive recipe.
"""

import argparse
import json
import os
import shutil
import tempfile
import zipfile


def copy(src, dst):
  os.makedirs(os.path.dirname(dst), exist_ok=True)
  shutil.copy2(src, dst)


def move(src, dst):
  os.makedirs(os.path.dirname(dst), exist_ok=True)
  shutil.move(src, dst)


def remove_empty_directories(directory):
  for root, dirs, files in os.walk(directory, topdown=False):
    del files
    for d in dirs:
      try:
        os.rmdir(os.path.join(root, d))
      except OSError:
        pass


def layout(archive_data, out_dir, base_dir):
  for z in archive_data.get('zipfiles', []):
    with zipfile.ZipFile(os.path.join(out_dir, z)) as zf:
      zf.extractall(out_dir)

  for f in archive_data.get('files', []):
    copy(os.path.join(out_dir, f), os.path.join(base_dir, f))

  for d in archive_data.get('dirs', []):
    shutil.copytree(
        os.path.join(out_dir, d), os.path.join(base_dir, d), dirs_exist_ok=True)

  for f in archive_data.get('rename_files', []):
    move(
        os.path.join(base_dir, f['from_file']),
        os.path.join(base_dir, f['to_file']))

  for d in archive_data.get('rename_dirs', []):
    shutil.move(
        os.path.join(base_dir, d['from_dir']),
        os.path.join(base_dir, d['to_dir']))

  remove_empty_directories(base_dir)


def package(name, json_path, out_dir, package_dir, print_contents):
  with open(json_path, encoding='utf-8') as j:
    package_json = json.load(j)

    for archive_data in package_json['archive_datas']:
      with tempfile.TemporaryDirectory() as tmp_dir:
        base_dir = os.path.join(tmp_dir, name)
        layout(archive_data, out_dir, base_dir)
        if print_contents:
          for _, _, paths in os.walk(base_dir):
            for path in paths:
              print(path)

        archive_type = archive_data['archive_type']
        if archive_type == 'ARCHIVE_TYPE_ZIP':
          shutil.make_archive(os.path.join(package_dir, name), 'zip', tmp_dir)
        elif archive_type == 'ARCHIVE_TYPE_FILES':
          shutil.copytree(base_dir, os.path.join(package_dir, name))


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--name', required=True, help='Name of package')
  parser.add_argument('--json_path', required=True, help='Package Json recipe')
  parser.add_argument(
      '--out_dir', required=True, help='Source of package content')
  parser.add_argument(
      '--package_dir', required=True, help='Destination of package')
  parser.add_argument(
      '--print_contents',
      action='store_true',
      default=False,
      help='Print package contents')
  args = parser.parse_args()

  package(args.name, args.json_path, args.out_dir, args.package_dir,
          args.print_contents)

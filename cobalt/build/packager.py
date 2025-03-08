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
https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/archive/properties.proto;drc=cca630e6c409dcdcc18567b94fcdc782b337e0ab;l=270
definition though Cobalt only implements a subset of the archive recipe.
"""

import argparse
import json
import os
import shutil
import tempfile


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


def layout(json_path, out_dir, base_dir):
  with open(json_path, encoding='utf-8') as j:
    package_json = json.load(j)
    archive_data = package_json['archive_datas'][0]

    files = archive_data.get('files')
    if files:
      for f in files:
        copy(os.path.join(out_dir, f), os.path.join(base_dir, f))

    rename_files = archive_data.get('rename_files')
    if rename_files:
      for f in rename_files:
        move(
            os.path.join(base_dir, f['from_file']),
            os.path.join(base_dir, f['to_file']))

    dirs = archive_data.get('dirs')
    if dirs:
      for d in dirs:
        shutil.copytree(
            os.path.join(out_dir, d),
            os.path.join(base_dir, d),
            dirs_exist_ok=True)

    rename_dirs = archive_data.get('rename_dirs')
    if rename_dirs:
      for d in rename_dirs:
        shutil.move(
            os.path.join(base_dir, d['from_dir']),
            os.path.join(base_dir, d['to_dir']))

  remove_empty_directories(base_dir)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--name', required=True, help='Name of package')
  parser.add_argument(
      '--json_path', required=True, help='Json recipe of package contents')
  parser.add_argument(
      '--out_dir', required=True, help='Source of package contents')
  parser.add_argument(
      '--package_dir', required=True, help='Destination of package')
  args = parser.parse_args()

  with tempfile.TemporaryDirectory() as tmp_dir:
    layout(args.json_path, args.out_dir, os.path.join(tmp_dir, args.name))
    shutil.make_archive(
        os.path.join(args.package_dir, args.name), 'zip', tmp_dir)

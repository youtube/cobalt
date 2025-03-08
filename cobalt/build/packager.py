#!/usr/bin/env python3
"""Packages Cobalt with a given layout."""

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

    files = archive_data['files']
    for f in files:
      copy(os.path.join(out_dir, f), os.path.join(base_dir, f))

    rename_files = archive_data['rename_files']
    for f in rename_files:
      move(
          os.path.join(base_dir, f['from_file']),
          os.path.join(base_dir, f['to_file']))

    dirs = archive_data['dirs']
    for d in dirs:
      shutil.copytree(
          os.path.join(out_dir, d),
          os.path.join(base_dir, d),
          dirs_exist_ok=True)

    rename_dirs = archive_data['rename_dirs']
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

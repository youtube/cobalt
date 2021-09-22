#!/usr/bin/env python3
#
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Formats ninja compilation databases to be easily diffed against each other.

Primarily used for looking at the differences between GYP and GN builds during
the GN migration.

To test, first generate the ".ninja" build files, then use

ninja -t compdb > out.json

in the build directory to generate a JSON file (out.json) containing all actions
ninja would run. Then, run this script on the out.json file to generate the
normalized_out.json file. Diff this normalized_out.json file with another
generated from GYP/GN to see differences in actions, build flags, etc.
"""

import argparse
import json
import os

from typing import List, Tuple

_ACTION_COMPONENTS = ['directory', 'command', 'file', 'output']
STRIP = 0


def make_path_absolute(path: str, directory: str) -> str:
  if os.path.isabs(path):
    return path

  return os.path.normpath(os.path.join(directory, path))


def remove_directory_path(path: str, directory: str) -> str:
  if STRIP:
    dirsplit = directory.split(os.path.sep)
    directory = os.path.sep.join(dirsplit[:-(STRIP)])
  if os.path.commonpath([path, directory]) != directory:
    return path

  return os.path.relpath(path, directory)


def normalize_if_pathlike(path: str, directory: str) -> str:
  absolute_path = make_path_absolute(path, directory)
  if os.path.exists(absolute_path):
    return remove_directory_path(absolute_path, directory)
  return path


def relativize_path(path: str, directory: str) -> str:
  path = make_path_absolute(path, directory)
  return remove_directory_path(path, directory)


def validate_json_database(json_content: object):
  for entry in json_content:
    for component in _ACTION_COMPONENTS:
      if component not in entry:
        raise ValueError(f'Ninja json entry is missing {component} field.')


def normalize_command(command: str, directory: str) -> Tuple[List[str], object]:
  command_list = command.split()
  command_with_normalized_paths = [
      normalize_if_pathlike(e, directory) for e in command_list
  ]

  # command_parser = argparse.ArgumentParser()
  # command_parser.add_argument('-x', type=str)
  # command_parser.add_argument('-MF', type=str)
  # command_parser.add_argument('-o', type=str)
  # command_parser.add_argument('-c', type=str)
  # parsed, unknown = command_parser.parse_known_args(
  #     command_with_normalized_paths)
  # return (parsed, sorted(unknown, reverse=True))
  return sorted(command_with_normalized_paths, reverse=True)


def normalize_all_paths(json_content: List[object]) -> List[object]:
  for entry in json_content:
    directory = entry['directory']
    command = normalize_command(entry['command'], directory)
    file_entry = relativize_path(entry['file'], directory)
    output_entry = entry['output']
    yield {'command': command, 'file': file_entry, 'output': output_entry}


def normalize_json_file(filename: str) -> List[object]:
  with open(filename) as f:
    content = json.load(f)

  validate_json_database(content)
  normalized_content = normalize_all_paths(content)
  return sorted(normalized_content, key=lambda x: x['output'])


def main(json_database: str, output_filename: str):
  json_content = normalize_json_file(json_database)
  with open(output_filename, 'w') as f:
    json.dump(json_content, f, indent=4, sort_keys=True)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('json_filename', type=str)
  parser.add_argument('-o', '--output', type=str)
  parser.add_argument('-p', '--strip', type=int)
  args = parser.parse_args()
  if args.strip:
    STRIP = int(args.strip)
  output = args.output if args.output else 'normalized_' + os.path.basename(
      args.json_filename)
  main(args.json_filename, output)

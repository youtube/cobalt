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
"""Prepares JUnit XML results for CI processing (merging and filtering)."""

import argparse
import glob
import logging
import os
import re
import shutil
import sys
from typing import Dict, List, Optional
import xml.etree.ElementTree as ET

import junit_xml_utils


def _get_attempt_from_path(file_path: str) -> int:
  """Extracts the attempt number from file path (e.g., 'attempt_2/...' -> 2)."""
  match = re.search(r'attempt_(\d+)', file_path)
  return int(match.group(1)) if match else 1


def _group_files_by_attempt(files: List[str]) -> Dict[int, List[str]]:
  """Groups a list of files by their attempt number."""
  files_by_attempt = {}
  for file_path in files:
    attempt = _get_attempt_from_path(file_path)
    files_by_attempt.setdefault(attempt, []).append(file_path)
  return files_by_attempt


def _merge_files_to_tree(files: List[str]) -> Optional[ET.ElementTree]:
  """Merges a list of XML files into a single ElementTree in memory."""
  if not files:
    return None

  try:
    tree = ET.parse(files[0])
    root = tree.getroot()
  except Exception as error:
    logging.error(f'Failed to parse {files[0]}: {error}')
    return None

  for file_path in files[1:]:
    try:
      t = ET.parse(file_path)
      junit_xml_utils.merge_trees(root, t.getroot())
    except Exception as error:
      logging.error(f'Failed to merge {file_path}: {error}')
      return None

  return tree


def _handle_pull_request(files_by_attempt: Dict[int, List[str]],
                         output_dir: str) -> int:
  """Handles merging of attempts for Pull Request events."""
  # Merge all files of attempt 1 in memory
  # We already verified attempt 1 exists and has right count in main()
  attempt_1_files = files_by_attempt[1]
  base_tree = _merge_files_to_tree(attempt_1_files)
  if not base_tree:
    return 1

  root_base = base_tree.getroot()

  # Merge subsequent attempts
  for attempt in sorted(files_by_attempt.keys()):
    if attempt == 1:
      continue

    retry_files = files_by_attempt[attempt]
    retry_tree = _merge_files_to_tree(retry_files)
    if not retry_tree:
      logging.error(f'Failed to merge files for attempt {attempt}')
      return 1

    junit_xml_utils.merge_trees(root_base, retry_tree.getroot())

  final_output = os.path.join(output_dir, 'final_merged.xml')
  try:
    base_tree.write(final_output, encoding='utf-8', xml_declaration=True)
    logging.info(f'Final merged report created at {final_output}')
    return 0
  except Exception as error:
    logging.error(f'Failed to write final merged report: {error}')
    return 1


def _handle_push(files_by_attempt: Dict[int, List[str]],
                 output_dir: str) -> int:
  """Handles filtering of failures for Push events."""
  for attempt, files in files_by_attempt.items():
    if attempt == 1:
      for file_path in files:
        filename = os.path.basename(file_path)
        output = os.path.join(output_dir, f'attempt_1_{filename}')
        shutil.copy(file_path, output)
    else:
      for file_path in files:
        filename = os.path.basename(file_path)
        output = os.path.join(output_dir,
                              f'filtered_attempt_{attempt}_{filename}')
        if not junit_xml_utils.filter_failures(file_path, output):
          logging.error(f'Failed to filter {file_path}')
          return 1
  return 0


def main() -> int:
  """Main function to prepare JUnit XMLs for CI."""
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

  parser = argparse.ArgumentParser(description='Prepare JUnit XMLs for CI.')
  parser.add_argument(
      '--glob', required=True, help='Glob pattern for XML files')
  parser.add_argument('--event_name', required=True, help='GitHub event name')
  parser.add_argument(
      '--output_dir', required=True, help='Directory to save processed files')
  parser.add_argument(
      '--num_shards', type=int, help='Expected number of shards for attempt 1')

  args = parser.parse_args()

  files = glob.glob(args.glob, recursive=True)
  logging.info(f'Found {len(files)} files matching {args.glob}')

  files_by_attempt = _group_files_by_attempt(files)

  # Verify shard count for attempt 1 (Testing and Minimalist suggestion)
  attempt_1_files = files_by_attempt.get(1, [])
  if args.num_shards and len(attempt_1_files) != args.num_shards:
    logging.error(
        f'Expected {args.num_shards} files for attempt 1, found {len(attempt_1_files)}'
    )
    return 1

  if args.event_name == 'pull_request':
    return _handle_pull_request(files_by_attempt, args.output_dir)
  elif args.event_name == 'push':
    return _handle_push(files_by_attempt, args.output_dir)
  else:
    logging.warning(
        f'Unsupported event type: {args.event_name}. Doing nothing.')
    return 0

  return 0


if __name__ == '__main__':
  sys.exit(main())

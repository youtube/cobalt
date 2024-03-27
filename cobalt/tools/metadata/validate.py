#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""
    Validates Cobalt METADATA files to schema, and applies Cobalt specific
    rules.
"""
import subprocess
import logging
from google.protobuf import text_format

import os
import sys
import argparse

log = logging.getLogger(__name__)


def validate_content(textproto_content,
                     metadata_file_path='',
                     warn_deprecations=False):
  # pylint: disable=import-outside-toplevel
  from cobalt.tools.metadata.gen import metadata_file_pb2
  metadata = metadata_file_pb2.Metadata()
  text_format.Parse(textproto_content, metadata)
  if not metadata.name:
    log.warning('%s: `name` field should be present', metadata_file_path)
  if not metadata.description:
    log.warning('%s: `description` field should be present', metadata_file_path)
  if not metadata.HasField('third_party'):
    raise RuntimeError('`third_party` field must be present')

  third_party = metadata.third_party
  if not third_party.license_type:
    log.warning('%s: third_party.licence_type is missing', metadata_file_path)
  if not third_party.version:
    log.warning('%s: third_party.version field should be present',
                metadata_file_path)
  if warn_deprecations and len(third_party.url) > 0:
    log.warning('"url" field is deprecated, please use "identifier" instead')

  git_id = next((id for id in third_party.identifier if id.type == 'Git'), None)
  is_internal = os.path.exists('internal')
  if git_id and not is_internal:  # Copybara doesn't preserve squash commits
    subtree_dir = os.path.dirname(metadata_file_path).replace(os.sep, '/')
    pattern = f'^git-subtree-dir: {subtree_dir}/*$'
    log_format = '%(trailers:key=git-subtree-split,valueonly)'
    args = ['git', 'log', '-1', f'--grep={pattern}', f'--pretty={log_format}']
    p = subprocess.run(args, capture_output=True, text=True, check=True)
    split = p.stdout.strip()
    if split and git_id.version != split:
      raise RuntimeError(f'{git_id.version} does not match {split}')


def validate_file(metadata_file_path, warn_deprecations=False):
  logging.info('Validating %s', metadata_file_path)
  with open(metadata_file_path, 'r', encoding='utf-8') as f:
    textproto_content = f.read()
    validate_content(textproto_content, metadata_file_path, warn_deprecations)


def update_proto():
  cur_dir = os.path.dirname(os.path.abspath(__file__))
  # Generate proto on the fly to ease development
  args = [
      'protoc', f'--proto_path={cur_dir}/protos',
      f'--python_out={cur_dir}/gen/', f'{cur_dir}/protos/metadata_file.proto'
  ]
  logging.info('Running %s', ' '.join(args))
  subprocess.run(args, text=True, check=False)


def filter_files(path):
  if not path.endswith(os.path.sep + 'METADATA'):
    return False
  if '.dist-info' in path:
    return False  # Python packaging files
  if os.path.join('skia', 'site') in path:
    return False  # Different upstream files
  return True


def main():
  parser = argparse.ArgumentParser(
      description=('Validate METADATA. If no'
                   ' arguments are passed, validate the entire tree'))
  parser.add_argument('files', nargs='*', help='File names')
  parser.add_argument(
      '-u',
      '--update_proto',
      action='store_true',
      help='Update generated proto definition')
  parser.add_argument(
      '-v', '--verbose', action='store_true', help='Verbose output')
  args = parser.parse_args()
  if args.verbose:
    logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
  if args.update_proto:
    update_proto()
  if args.files:
    _ = [validate_file(file) for file in args.files if filter_files(file)]
  else:  # Run all
    args = ['git', 'ls-files']
    p = subprocess.run(args, capture_output=True, text=True, check=True)
    for f in p.stdout.splitlines():
      if filter_files(f):
        validate_file(f)


if __name__ == '__main__':
  main()

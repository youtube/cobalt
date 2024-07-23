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
try:
  from cobalt.tools.metadata.gen import metadata_file_pb2
except ImportError:
  metadata_file_pb2 = None
  pass

log = logging.getLogger(__name__)

CHROMIUM_HOST = 'https://chromium.googlesource.com'
CHROMIUM_SRC = CHROMIUM_HOST + '/chromium/src.git'

<<<<<<< HEAD
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
=======

def is_full_git_hash(s):
  pattern = r'^[0-9a-fA-F]{40}$'
  return re.match(pattern, s) is not None


class MetaData(object):
  """Validates Metadadata fields"""

  def __init__(self, textproto_content, metadata_file_path):
    if not metadata_file_pb2:
      raise RuntimeError('Unable to load METADATA schema, please re-generate'
                         ' by running `validate.py -u`')
    metadata = metadata_file_pb2.Metadata()
    text_format.Parse(textproto_content, metadata)
    if not metadata.name:
      raise RuntimeError(f'{metadata_file_path}: `name` field must be present')
    self.name = metadata.name
    if not metadata.HasField('third_party'):
      raise RuntimeError(
          f'{metadata_file_path}: `third_party` field must be present')

    self.third_party = metadata.third_party
    if not self.third_party.license_type:
      log.warning('%s: third_party.licence_type is missing', metadata_file_path)
    if len(self.third_party.url) > 0:
      log.warning('"url" field is deprecated, please use "identifier" instead')

    def identifier(name):
      return next((id for id in self.third_party.identifier if id.type == name),
                  None)

    self.git_id = identifier('Git')
    if not self.git_id:
      raise RuntimeError(
          f'{metadata_file_path}: identifier \'Git\' must be present')
    self.closest_version = self.git_id.closest_version

    if not self.git_id.version:
      raise RuntimeError(
          f'{metadata_file_path}: third_party.version field for identifier '
          f'\'Git\' must be present')
    if not self.git_id.value:
      raise RuntimeError(
          f'{metadata_file_path}: third_party.value field for identifier '
          f'\'Git\' must be present')
    self.chromium_version = identifier('ChromiumVersion')
    if self.chromium_version:
      self.chromium_version = self.chromium_version.value
    else:
      self.chromium_version = ''
    self.upstream_subdir = identifier('UpstreamSubdir')
    if self.upstream_subdir:
      self.upstream_subdir = self.upstream_subdir.value
    else:
      self.upstream_subdir = ''
    self.subpackages = identifier('SubPackages')
    if self.subpackages:
      self.omitted_dirs = self.subpackages.value.split(',')
    else:
      self.omitted_dirs = ''
    self.is_chromium = self.git_id.value == CHROMIUM_SRC
    self.from_chromium = self.git_id.value.startswith(CHROMIUM_HOST)
    self.file = metadata_file_path
    self.dir = os.path.dirname(self.file)
    self.in_3p = 'third_party/' in self.file
    self.closet_version = self.git_id.closest_version or ''
    self.git_hash = self.git_id.version
    self.git_repo = self.git_id.value


def validate_content(textproto_content, metadata_file_path=''):
  meta = MetaData(textproto_content, metadata_file_path)
>>>>>>> 8630a281628 (Add list command to Metadata validation (#3863))
  is_internal = os.path.exists('internal')
  if git_id and not is_internal:  # Copybara doesn't preserve squash commits
    subtree_dir = os.path.dirname(metadata_file_path).replace(os.sep, '/')
    pattern = f'^git-subtree-dir: {subtree_dir}/*$'
    log_format = '%(trailers:key=git-subtree-split,valueonly)'
    args = ['git', 'log', '-1', f'--grep={pattern}', f'--pretty={log_format}']
    p = subprocess.run(args, capture_output=True, text=True, check=True)
    split = p.stdout.strip()
    if split and meta.git_id.version != split:
      raise RuntimeError(f'{meta.git_id.version} does not match {split}')


def validate_file(metadata_file_path):
  logging.info('Validating %s', metadata_file_path)
  with open(metadata_file_path, 'r', encoding='utf-8') as f:
    textproto_content = f.read()
    validate_content(textproto_content, metadata_file_path)


def list_file(metadata_file_path, format_tuple, apply_filters):
  logging.info('Listing %s', metadata_file_path)
  with open(metadata_file_path, 'r', encoding='utf-8') as f:
    textproto_content = f.read()
    meta = MetaData(textproto_content, metadata_file_path)
    for filter_ in apply_filters:
      if filter_(meta):
        return
    fmt_string = format_tuple[0]
    values_tuple = format_tuple[2](meta)
    print(fmt_string.format(*values_tuple))


def runstats(meta):
  file_count = 0
  dir_count = 0
  for _, dirs, files in os.walk(meta.dir):
    file_count += len(files)
    dir_count += len(dirs)

  files_changed = 'unknown'
  fraction = 'unknown'
  insertions = 'unknown'
  deletions = 'unknown'

  subtree_dir = os.path.dirname(meta.file).replace(os.sep, '/')
  args = [
      'git', 'log', f'--grep=^git-subtree-dir: {subtree_dir}/*$', '-1',
      '--pretty=%H'
  ]
  p = subprocess.run(args, capture_output=True, text=True, check=True)
  split = p.stdout.strip()

  if split:
    args = ['git', 'diff', split, f'@:{subtree_dir}', ':(exclude)METADATA']
    for omit_dir in meta.omitted_dirs:
      args.append(f':(exclude){omit_dir}')
    f = subprocess.run(
        args, capture_output=True, text=True, errors='replace', check=True)
    diff_output = f.stdout.strip()
    result = subprocess.run(['diffstat', '-s'],
                            input=diff_output,
                            text=True,
                            capture_output=True,
                            check=True)
    diffstat = result.stdout.strip()
    match = re.search(
        r'(\d+) files? changed,?'
        r'(?: (\d+) insertions?,?)?'
        r'(?:.*(\d+) deletion)?', diffstat)
    if match:
      files_changed = int(match.group(1))
      fraction = round((files_changed / file_count) * 100, 2)
      if files_changed == 0:
        insertions = 0
        deletions = 0
      else:
        if match.group(2):
          insertions = int(match.group(2))
        else:
          insertions = 0
        if match.group(3):
          deletions = int(match.group(3))
        else:
          deletions = 0
  return (dir_count, file_count, files_changed, insertions, deletions, fraction)


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

  subparsers = parser.add_subparsers(dest='command', help='Subcommand to run')
  list_parser = subparsers.add_parser(
      'list',
      help='List the packages',
      formatter_class=argparse.RawTextHelpFormatter)
  list_parser.add_argument(
      '--filter',
      type=str,
      nargs='+',
      choices=[
          'subdir', 'subdir-', 'chromium', 'chromium-', 'chromium_copy',
          'chromium_copy-', '3p', '3p-', 'identified', 'identified-', 'target',
          'target-'
      ],
      help='''Filter items based on criteria. Append - to exclude.
        Multiple filters can be used.
        Choices are:
        subdir/subdir-: Include/exclude packages that are subdirectory imports.
        chromium/chromium-: Include/exclude packages from Chromium root repo.
        chromium_copy/chromium_copy-: Include/exclude packages that are hosted
          on Chromium Gerrit.
        3p/3p-: Include/exclude packages under third_party dirs.
        identified/identified-: Has a full origin Git ref tracing back to origin
        target/target-: Include/exclude packages that are at the target
          Chromium version''')
  list_parser.add_argument(
      '--format',
      choices=['all', 'dir', 'versions', 'stats'],
      default='all',
      help='''Columns to output
        all: Shows all.
        dir: Only columns relevant to directory structure.
        versions: Only columns having version info.
        stats: Show per-package file modification stats ( slow ).
        ''')

  args = parser.parse_args()
  if args.verbose:
    logging.basicConfig(stream=sys.stdout, level=logging.DEBUG)
  if args.update_proto:
    update_proto()

  do_list = args.command == 'list'
  if do_list:
    version_file = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), 'VERSION.chromium')
    with open(version_file, 'r', encoding='utf-8') as f:
      target_ver = f.read().strip()

    package_filter = args.filter or []
    # Filter functions
    filters = {
        'subdir': lambda meta: (meta.upstream_subdir),
        'chromium': lambda meta: (meta.is_chromium),
        'chromium_copy': lambda meta: (meta.from_chromium),
        '3p': lambda meta: (meta.in_3p),
        'identified': lambda meta: (is_full_git_hash(meta.git_hash)),
        'target': lambda meta: (meta.chromium_version == target_ver),
    }
    apply_filters = [
        lambda meta, fn=v: not fn(meta)
        for (k, v) in filters.items()
        if k in package_filter
    ]
    apply_filters += [
        lambda meta, fn=v: fn(meta)
        for (k, v) in filters.items()
        if k + '-' in package_filter
    ]
    # This isn't a separate package
    apply_filters += [lambda meta: meta.dir == 'third_party']
    # Printing output formats
    formats = {
        'all': ('{0:<30} {1:<40} {2:<40} {3:<20} {4:<20} {5:40} {6}',
                ('name', 'dir', 'upstream_dir', 'chromium_version',
                 'closest_version', 'git hash', 'git repo'), lambda meta: (
                     meta.name,
                     meta.dir,
                     meta.upstream_subdir,
                     meta.chromium_version,
                     meta.closest_version,
                     meta.git_hash,
                     meta.git_repo,
                 )),
        'dir': ('{0:<30} {1:<40} {2:<40}', ('name', 'dir', 'upstream_dir'),
                lambda meta: (meta.name, meta.dir, meta.upstream_subdir)),
        'versions': ('{0:<40} {1:<30} {2:<20} {3:<20} {4:40}',
                     ('dir', 'name', 'chromium_version', 'closest_version',
                      'git hash'), lambda meta:
                     (meta.dir, meta.name, meta.chromium_version, meta.
                      closest_version, meta.git_hash)),
        'stats':
            ('{0:<30} {1:<40} {2:<10} {3:<10} {4:<10} {5:<10} {6:<10} {7:<10}',
             ('name', 'dir', 'dirs', 'files', 'changed', 'insertions',
              'deletions', '% modified files'), lambda meta:
             (meta.name, meta.dir) + runstats(meta)),
    }
    format_tuple = formats[args.format]
    fmt_string = format_tuple[0]
    header_names = format_tuple[1]
    print(fmt_string.format(*header_names))

  if args.files:
    _ = [validate_file(file) for file in args.files if filter_files(file)]
  else:  # Run all
    cmdargs = ['git', 'ls-files']
    p = subprocess.run(cmdargs, capture_output=True, text=True, check=True)
    for f in p.stdout.splitlines():
      if filter_files(f):
        if do_list:
          list_file(f, format_tuple, apply_filters)
        else:
          validate_file(f)


if __name__ == '__main__':
  main()

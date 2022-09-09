#!/usr/bin/python
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Tools for creating a compressed tar archive.

Usage:
  create_archive.py -d <dest_path> -s <source_paths>

Args:
    dest_path: Path (can be absolute or relative) to the destination folder
      where the compressed archive is created.
    source_paths: Paths (can be absolute or relative) to the source folders
      which should archived.

Returns:
    0 on success.

Raises:
    ValueError: An error occurred when adding a source to the archive, when
      compressing the archive, or when the archive has uppercase symlink names.
"""

import argparse
import glob
import logging
import os
import subprocess
import sys
import time

import worker_tools

# Use strict include filter to pass artifacts to Mobile Harness
TEST_PATTERNS = [
    'content/*',
    'deploy/*',
    'install/*',
    # TODO: All of those should be built under deploy/
    '*.apk',
    'ds_archive.zip',
    'app_launcher.zip',
    'crashpad_handler',
    'elf_loader_sandbox'
]

# Reference: https://sevenzip.osdn.jp/chm/cmdline/switches/
_7Z_PATH = r'%ProgramFiles%\7-Zip\7z.exe'  # pylint:disable=invalid-name
# 1 is fastest and least compression, max is 9, see -mx parameter in docs
_COMPRESSION_LEVEL = 1

_LINUX_PARALLEL_ZIP_EXTENSION = 'tar.xz'
_LINUX_PARALLEL_ZIP_PROGRAM = 'xz -T0'
_LINUX_SERIAL_ZIP_EXTENSION = 'tar.gz'
_LINUX_SERIAL_ZIP_PROGRAM = 'gzip'


# TODO(b/143570416): Refactor this method and similar ones in a common file.
def _IsWindows():
  return sys.platform in ['win32']


def _CheckArchiveExtension(archive_path, is_parallel):
  if is_parallel and (_LINUX_PARALLEL_ZIP_EXTENSION not in archive_path):
    raise RuntimeError(
        'Invalid archive extension. Parallelized zip requested, but path is %s'
        % archive_path)
  if not is_parallel and (_LINUX_PARALLEL_ZIP_EXTENSION in archive_path):
    raise RuntimeError(
        'Invalid archive extension. Serialized zip requested, but path is: %s' %
        archive_path)


def _CreateCompressedArchive(source_paths,
                             dest_path,
                             patterns,
                             intermediate,
                             is_parallel=False):
  """Create a compressed tar archive.

  This function creates a compressed tar archive from all of the source
  directories in |source_paths|. The basename of each source directory is
  maintained within the archive to prevent file overwriting on extraction.

  Args:
    source_paths: Paths (that can be absolute or relative) to the source folders
      which should be archived.
    dest_path: Path (can be absolute or relative) that the compressed archive
      should be created at.
    patterns: Patterns of allowable files to archive.
    intermediate: Flag to archive intermediate build artifacts needed by
      Buildbot.

  Raises:
    ValueError: An error occurred when adding a source to the archive, when
      compressing the archive, or when the archive has uppercase symlink names.
  """
  # Ensure the directory where we will create our archive exists, and that the
  # directory does not contain a previous archive.
  dest_parent = os.path.dirname(os.path.abspath(dest_path))
  if os.path.exists(dest_path):
    os.remove(dest_path)
  elif not os.path.exists(dest_parent):
    os.makedirs(dest_parent)

  # Get the current working directory to return to once we are done archiving.
  cwd = os.getcwd()

  _CheckArchiveExtension(dest_path, is_parallel)

  try:
    intermediate_tar_path = os.path.join(
        dest_parent, 'create_archive_intermediate_artifacts.tar')

    # Generate the commands to archive each source directory and execute them
    # from the parent directory of the source we are adding to the archive.
    for source_path in source_paths:
      # Determine if only certain contents of the source_path are to be
      # included.
      if ',' in source_path:
        source_path_parts = source_path.split(',')
        source_path = source_path_parts[0]
        included_paths = source_path_parts[1:]
      else:
        included_paths = ['']

      logging.info('source path: %s', source_path)
      logging.info('included subpaths: %s', included_paths)

      if not os.path.exists(source_path):
        raise ValueError('Failed to find source path "{}".'.format(source_path))

      for target_path in included_paths:
        if not os.path.exists(os.path.join(source_path, target_path)):
          raise ValueError(
              'Failed to find target path "{}".'.format(target_path))

      source_parent_dir, source_dir_name = os.path.split(
          os.path.abspath(source_path))

      os.chdir(source_parent_dir)
      t_start = time.time()
      for included_path in included_paths:
        # Check every path to make sure there is no symlink being created with
        # uppercase in it. This causes issues in Windows Archives.
        for dirpath, dirnames, _ in os.walk(included_path):
          for dirname in dirnames:
            iterpath = os.path.join(dirpath, dirname)
            if os.path.islink(iterpath) and iterpath.lower() != iterpath:
              raise ValueError('Archive contains uppercase symlink names.\n'
                               'Please change these names to lowercase.')
        # Iteratively add filtered source files to the final tar
        _AddSourceToTar(
            os.path.join(source_dir_name, included_path), intermediate_tar_path,
            patterns, intermediate)
        t_archive = time.time() - t_start
        logging.info('Archiving took %5.2f seconds', t_archive)
      os.chdir(cwd)

    archive_size = os.path.getsize(intermediate_tar_path) / 1e9
    logging.info('Size of %s: %5.2f Gi', intermediate_tar_path, archive_size)
    # Compress the archive.
    cmd = _CreateZipCommand(intermediate_tar_path, dest_path, is_parallel)
    logging.info('Compressing "%s" into %s with "%s".', intermediate_tar_path,
                 dest_path, cmd)
    t_start = time.time()
    subprocess.check_call(cmd, shell=True)
    t_compress = time.time() - t_start
    logging.info('Compressing took %5.2f seconds', t_compress)
    archive_size = os.path.getsize(dest_path) / 1e9
    logging.info('Size of %s: %5.2f Gi', dest_path, archive_size)
  finally:
    if os.path.exists(intermediate_tar_path):
      os.remove(intermediate_tar_path)


def UncompressArchive(source_path, dest_path, is_parallel=False):
  """Uncompress tar archive.

  Args:
    source_path: Path (that can be absolute or relative) to the archive.
    dest_path: Path (can be absolute or relative) where the archive should be
      uncompressed to.
  """
  _CheckArchiveExtension(source_path, is_parallel)

  # Creates and cleans up destination path
  dest_path = os.path.abspath(dest_path)
  if not os.path.exists(dest_path):
    os.makedirs(dest_path)

  try:
    intermediate_tar_path = os.path.join(
        dest_path, 'create_archive_intermediate_artifacts.tar')

    # Unzips archive
    cmd = _CreateUnzipCommand(source_path, intermediate_tar_path, is_parallel)
    logging.info('Unzipping "%s" into %s with "%s".', source_path,
                 intermediate_tar_path, cmd)
    subprocess.check_call(cmd, shell=True)

    # Untars archive
    cmd = _CreateUntarCommand(intermediate_tar_path)
    logging.info('Untarring "%s" into "%s" with "%s".', intermediate_tar_path,
                 dest_path, cmd)
    subprocess.check_call(cmd, shell=True, cwd=dest_path)
  finally:
    os.remove(intermediate_tar_path)


def _AddSourceToTar(source_path, intermediate_tar_path, patterns, intermediate):
  """Add a directory to an archive.

  The tarfile Python 2.7 module does not work with symlinks, so the operation
  is done in platform-specific ways.

  Args:
    source_path: Source directory to add to archive.
    intermediate_tar_path: Path to tar file to add to (or create).
    patterns: Patterns of allowable files to archive.
    intermediate: Flag to archive intermediate build artifacts needed by
      Buildbot.
  """
  cmd = _CreateTarCommand(source_path, intermediate_tar_path, patterns,
                          intermediate)
  logging.info('Adding "%s" to "%s" with "%s".', source_path,
               intermediate_tar_path, cmd)
  subprocess.check_call(cmd, shell=True)


def _CreateTarCommand(source_path, intermediate_tar_path, patterns,
                      intermediate):
  if _IsWindows():
    return _CreateWindowsTarCmd(source_path, intermediate_tar_path, patterns,
                                intermediate)
  return _CreateLinuxTarCmd(source_path, intermediate_tar_path, patterns,
                            intermediate)


def _CreateWindowsTarCmd(source_path, intermediate_tar_path, patterns,
                         intermediate):
  exclude_patterns = worker_tools.INTERMEDIATE_FILE_NAMES_WILDCARDS
  if intermediate:
    exclude_patterns = worker_tools.BUILDBOT_INTERMEDIATE_FILE_NAMES_WILDCARDS
  excludes = ' '.join([
      '-xr!{}'.format(x.replace(worker_tools.SOURCE_DIR, source_path))
      for x in exclude_patterns
  ])
  contents = [
      os.path.join(source_path, pattern)
      for pattern in patterns
      if glob.glob(os.path.join(source_path, pattern))
  ]
  return '"{}" a {} -bsp1 -snl -ttar {} {}'.format(_7Z_PATH, excludes,
                                                   intermediate_tar_path,
                                                   ' '.join(contents))


def _CreateLinuxTarCmd(source_path, intermediate_tar_path, patterns,
                       intermediate):
  exclude_patterns = worker_tools.INTERMEDIATE_FILE_NAMES_WILDCARDS
  if intermediate:
    exclude_patterns = worker_tools.BUILDBOT_INTERMEDIATE_FILE_NAMES_WILDCARDS
  excludes = ' '.join([
      '--exclude="{}"'.format(x.replace(worker_tools.SOURCE_DIR, source_path))
      for x in exclude_patterns
  ])
  mode = 'r' if os.path.exists(intermediate_tar_path) else 'c'
  contents = [
      os.path.join(source_path, pattern)
      for pattern in patterns
      if glob.glob(os.path.join(source_path, pattern))
  ]
  return 'tar -{}vf {} --format=posix {} {}'.format(mode, intermediate_tar_path,
                                                    excludes,
                                                    ' '.join(contents))


def _CreateUntarCommand(intermediate_tar_path):
  if _IsWindows():
    return '"{}" x -bsp1 {}'.format(_7Z_PATH, intermediate_tar_path)
  else:
    return 'tar -xvf {}'.format(intermediate_tar_path)


def _CreateZipCommand(intermediate_tar_path, dest_path, is_parallel=False):
  if _IsWindows():
    return '"{}" a -bsp1 -mx={} -mmt=on {} {}'.format(_7Z_PATH,
                                                      _COMPRESSION_LEVEL,
                                                      dest_path,
                                                      intermediate_tar_path)
  else:
    zip_program = (
        _LINUX_PARALLEL_ZIP_PROGRAM
        if is_parallel else _LINUX_SERIAL_ZIP_PROGRAM)
    return '{} -vc -1 {} > {}'.format(zip_program, intermediate_tar_path,
                                   dest_path)


def _CreateUnzipCommand(source_path, intermediate_tar_path, is_parallel=False):
  if _IsWindows():
    tar_parent = os.path.dirname(intermediate_tar_path)
    return '"{}" x -bsp1 {} -o{}'.format(_7Z_PATH, source_path, tar_parent)
  else:
    zip_program = (
        _LINUX_PARALLEL_ZIP_PROGRAM
        if is_parallel else _LINUX_SERIAL_ZIP_PROGRAM)
    return '{} -dvc {} > {}'.format(zip_program, source_path,
                                    intermediate_tar_path)


def _CleanUp(intermediate_tar_path):
  if os.path.exists(intermediate_tar_path):
    os.remove(intermediate_tar_path)


def main():
  logging_format = '[%(levelname)s:%(filename)s:%(lineno)s] %(message)s'
  logging.basicConfig(
      level=logging.INFO, format=logging_format, datefmt='%H:%M:%S')

  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-d',
      '--dest_path',
      type=str,
      required=True,
      help='The path to create the archive at.')
  parser.add_argument(
      '-s',
      '--source_paths',
      type=str,
      nargs='+',
      required=True,
      help='The paths to the source folders which should be archived. '
      'To include only specific subdirectories, but compress relative '
      'to some root directory, pass: "<root>,<target_subdir>,...".')
  parser.add_argument(
      '-x',
      '--extract',
      default=False,
      action='store_true',
      help='Uncompresses tar archive.')
  parser.add_argument(
      '-p',
      '--patterns',
      type=str,
      nargs='+',
      required=False,
      default=['*'],
      help='The patterns of allowable files to archive. Defaults to *.')
  parser.add_argument(
      '--test_infra',
      default=False,
      action='store_true',
      help='Utilizes predefined test infrastructure patterns.')
  parser.add_argument(
      '--intermediate',
      default=False,
      action='store_true',
      help='Archives intermediate build artifacts needed by Buildbot.')
  parser.add_argument(
      '--parallel',
      default=False,
      action='store_true',
      help='Archives using parallelized methods.')
  args = parser.parse_args()

  worker_tools.MoveToWindowsShortSymlink()
  if args.extract:
    UncompressArchive(args.source_paths[0], args.dest_path, args.parallel)
  else:
    _CreateCompressedArchive(
        args.source_paths, args.dest_path,
        TEST_PATTERNS if args.test_infra else args.patterns,
        args.intermediate, args.parallel)


if __name__ == '__main__':
  main()

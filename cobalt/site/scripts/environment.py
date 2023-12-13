# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Common variables and functions for site generation."""

import argparse
from contextlib import contextmanager
import errno
import logging
import os
import shutil
import tempfile
import textwrap

SCRIPTS_DIR = os.path.abspath(os.path.dirname(__file__))
COBALT_SOURCE_DIR = os.path.abspath(
    os.path.join(*([SCRIPTS_DIR] + 5 * [os.pardir])))


@contextmanager
def mkdtemp(suffix='', prefix='tmp.', base_dir=None):
  temp_directory_path = tempfile.mkdtemp(suffix, prefix, base_dir)
  try:
    yield temp_directory_path
  finally:
    if temp_directory_path:
      try:
        shutil.rmtree(temp_directory_path)
      except IOError:
        logging.warning('Failed to delete temp directory: %s',
                        temp_directory_path)
      else:
        logging.info('Deleted temp directory: %s', temp_directory_path)


def read_lines(path):
  with open(path, 'r', encoding='utf8') as contents:
    return contents.readlines()


def read_file(path):
  with open(path, 'r', encoding='utf8') as contents:
    return contents.read()


def write_file(path, contents):
  with open(path, 'w', encoding='utf8') as output:
    output.write(contents)


def setup_logging(default_level=logging.INFO):
  logging_level = default_level
  logging_format = '%(asctime)s.%(msecs)03d [%(levelname)-8s] %(message)s'
  datetime_format = '%H:%M:%S'
  logging.basicConfig(
      level=logging_level, format=logging_format, datefmt=datetime_format)


_LOG_LEVELS = [
    logging.CRITICAL,
    logging.ERROR,
    logging.WARNING,
    logging.INFO,
    logging.DEBUG,
]


def set_log_level(log_delta):
  """Sets the log level based on the log delta from default."""
  logger = logging.getLogger()
  try:
    level_index = _LOG_LEVELS.index(logger.getEffectiveLevel())
  except ValueError:
    level_index = _LOG_LEVELS.index(logging.INFO)

  level_index = min(len(_LOG_LEVELS) - 1, max(0, level_index + log_delta))
  logging.getLogger().setLevel(_LOG_LEVELS[level_index])


def parse_arguments(doc_string, argv):
  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
      description=textwrap.dedent(doc_string))
  parser.add_argument(
      '-s',
      '--source',
      type=str,
      default=COBALT_SOURCE_DIR,
      help='The repository root that contains the source code to parse.')
  parser.add_argument(
      '-o',
      '--out',
      type=str,
      default=None,
      help='The root of the directory to output to.')
  parser.add_argument(
      '-v',
      '--verbose',
      dest='verbose_count',
      default=0,
      action='count',
      help='Verbose level (multiple times for more).')
  parser.add_argument(
      '-q',
      '--quiet',
      dest='quiet_count',
      default=0,
      action='count',
      help='Quietness level (multiple times for more).')
  options = parser.parse_args(argv)
  options.log_delta = options.verbose_count - options.quiet_count
  return options


def get_cobalt_dir(source_dir):
  return os.path.join(source_dir, 'cobalt')


def get_cobalt_build_dir(source_dir):
  return os.path.join(get_cobalt_dir(source_dir), 'build')


def get_site_dir(source_dir):
  return os.path.join(get_cobalt_dir(source_dir), 'site')


def get_starboard_dir(source_dir):
  return os.path.join(source_dir, 'starboard')


def get_starboard_build_dir(source_dir):
  return os.path.join(get_starboard_dir(source_dir), 'build')


def get_stub_platform_dir(source_dir):
  return os.path.join(get_starboard_dir(source_dir), 'stub')


def make_dirs(path):
  """Make the specified directory and any parents in the path."""
  if path and not os.path.isdir(path):
    make_dirs(os.path.dirname(path))
    try:
      os.mkdir(path)
    except OSError as e:
      if e.errno != errno.EEXIST:
        raise
      pass


def make_clean_dirs(path):
  """Make the specified directory and any parents in the path."""
  make_dirs(os.path.dirname(path))
  shutil.rmtree(path, ignore_errors=True)
  make_dirs(path)

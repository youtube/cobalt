#!/usr/bin/env python3
# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
"""Calculates the current Build ID and writes it to 'build.id'."""

import argparse
import logging
import os
import sys
import textwrap

from cobalt.build import get_build_id
from cobalt.tools import paths

_BUILD_ID_PATH = os.path.join(paths.BUILD_ROOT, 'build.id')
_SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))

# Return values used by main().
RETVAL_SUCCESS = 0
RETVAL_ERROR = 1


def main():
  logging.basicConfig(level=logging.WARNING, format='%(message)s')

  parser = argparse.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
      description=textwrap.dedent(__doc__))

  parser.add_argument(
      '--delete',
      '-d',
      action='store_true',
      default=False,
      help='Delete build.id file.')

  parser.add_argument(
      '--build-id',
      '-b',
      type=int,
      default=0,
      help='Build id to save. If not passed, value will be loaded remotely.')

  options = parser.parse_args()

  # Update the build id to the latest, even if one is already set.
  try:
    os.unlink(_BUILD_ID_PATH)
  except OSError:
    pass

  if options.delete:
    return 0

  if not options.build_id:
    options.build_id = get_build_id.main()

  if not options.build_id:
    logging.error('Unable to retrieve build id.')
    return RETVAL_ERROR

  try:
    with open(_BUILD_ID_PATH, 'w', encoding='utf-8') as build_id_file:
      build_id_file.write(f'{options.build_id}')
  except RuntimeError as e:
    logging.error(e)
    return RETVAL_ERROR

  return RETVAL_SUCCESS


if __name__ == '__main__':
  sys.exit(main())

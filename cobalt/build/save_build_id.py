#!/usr/bin/env python
# Copyright 2016 Google Inc. All Rights Reserved.
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

import logging
import os
import sys

import gyp_utils


_SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))
_BUILD_ID_PATH = gyp_utils.BUILD_ID_PATH

# Return values used by main().
RETVAL_SUCCESS = 0
RETVAL_ERROR = 1


def main():
  logging.basicConfig(level=logging.WARNING, format='%(message)s')

  # Update the build id to the latest, even if one is already set.
  try:
    os.unlink(_BUILD_ID_PATH)
  except OSError:
    pass

  build_id = gyp_utils.GetBuildNumber()
  if not build_id:
    logging.error('Unable to retrieve build id.')
    return RETVAL_ERROR

  try:
    with open(_BUILD_ID_PATH, 'w') as build_id_file:
      build_id_file.write('{0}'.format(build_id))
  except RuntimeError as e:
    logging.error(e)
    return RETVAL_ERROR

  return RETVAL_SUCCESS


if __name__ == '__main__':
  sys.exit(main())

#!/usr/bin/python
#
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
"""Provides a consistent mechanism for the initialize of logging."""

import logging
import _env  # pylint: disable=unused-import

_NAME_TO_LEVEL = {
    'info': logging.INFO,
    'debug': logging.DEBUG,
    'warning': logging.WARNING,
    'error': logging.ERROR,
    'critical': logging.CRITICAL,
}


def InitializeLogging(args):
  """Parses the provided argparse.Namespace to determine a logging level."""
  log_level = logging.INFO

  if args:
    # Allow a 'verbose' flag to force |logging.DEBUG|.
    if 'verbose' in args and args.verbose:
      log_level = logging.DEBUG
    elif 'log_level' in args:
      log_level = _NAME_TO_LEVEL[args.log_level]

  InitializeLoggingWithLevel(log_level)


def InitializeLoggingWithLevel(log_level):
  logging.basicConfig(
      level=log_level,
      format=('[%(process)d:%(asctime)s.%(msecs)03d...:'
              '%(levelname)s:%(filename)s(%(lineno)s)] %(message)s'),
      datefmt='%m-%d %H:%M')

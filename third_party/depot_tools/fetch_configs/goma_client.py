# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import config_util  # pylint: disable=import-error


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=no-init
class GomaClient(config_util.Config):
  """Basic Config class for Goma client."""

  @staticmethod
  def fetch_spec(_props):
    return {
      'type': 'gclient_git',
      'gclient_git_spec': {
        'solutions': [
          {
            'name'     : 'client',
            'url'      : 'https://chromium.googlesource.com/infra/goma/client.git',
            'deps_file': 'DEPS',
            'managed'  : False,
          }
        ],
      },
    }

  @staticmethod
  def expected_root(_props):
    return 'client'


def main(argv=None):
  return GomaClient().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

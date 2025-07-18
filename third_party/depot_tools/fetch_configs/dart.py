# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import config_util  # pylint: disable=import-error


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=no-init
class Dart(config_util.Config):
  """Basic Config class for Dart."""

  @staticmethod
  def fetch_spec(props):
    url = 'https://dart.googlesource.com/sdk.git'
    solution = {
      'name'   :'sdk',
      'url'    : url,
      'deps_file': 'DEPS',
      'managed'   : False,
      'custom_deps': {},
    }
    spec = {
      'solutions': [solution],
    }
    if props.get('target_os'):
      spec['target_os'] = props['target_os'].split(',')
    return {
      'type': 'gclient_git',
      'gclient_git_spec': spec,
    }

  @staticmethod
  def expected_root(_props):
    return 'sdk'


def main(argv=None):
  return Dart().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))

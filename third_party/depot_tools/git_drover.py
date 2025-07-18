#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse


_HELP_MESSAGE = """\
git drover has been deprecated in favor of cherry-picking using Gerrit.
Try it, it's faster!

See https://www.chromium.org/developers/how-tos/drover for instructions.

If the Gerrit UI is not sufficient, and you know what you're doing:
  git checkout -b branch-name refs/remotes/branch-heads/{branch}
  git cherry-pick -x {cherry_pick}

If you have to do a lot of merges, consider using multiple working directories
in your checkout:
https://www.chromium.org/developers/how-tos/get-the-code/multiple-working-directories
"""


def main():
  parser = argparse.ArgumentParser(description=_HELP_MESSAGE)
  parser.add_argument(
      '--branch',
      default='BRANCH',
      metavar='BRANCH',
      type=str,
      help='the name of the branch to which to cherry-pick; e.g. 1234')
  parser.add_argument(
      '--cherry-pick',
      default='HASH_OF_THE_COMMIT_TO_CHERRY_PICK',
      metavar='HASH_OF_THE_COMMIT_TO_CHERRY_PICK',
      type=str,
      help=('the change to cherry-pick; this can be any string '
            'that unambiguosly refers to a revision not involving HEAD'))
  options, _ = parser.parse_known_args()

  print(_HELP_MESSAGE.format(
      branch=options.branch, cherry_pick=options.cherry_pick))

if __name__ == '__main__':
  main()

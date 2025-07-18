# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess


def depot_tools_version():
  depot_tools_root = os.path.dirname(os.path.abspath(__file__))
  try:
    commit_hash = subprocess.check_output(['git', 'rev-parse', 'HEAD'],
                                          cwd=depot_tools_root).decode(
                                              'utf-8', 'ignore')
    return 'git-%s' % commit_hash
  except Exception:
    pass

  # git check failed, let's check last modification of frequently checked file
  try:
    mtime = os.path.getmtime(
        os.path.join(depot_tools_root, 'infra', 'config', 'recipes.cfg'))
    return 'recipes.cfg-%d' % (mtime)
  except Exception:
    return 'unknown'

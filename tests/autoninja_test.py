#!/usr/bin/env python3
# Copyright (c) 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing
import os
import os.path
import sys
import unittest
import unittest.mock

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import autoninja
from testing_support import trial_dir


def write(filename, content):
  """Writes the content of a file and create the directories as needed."""
  filename = os.path.abspath(filename)
  dirname = os.path.dirname(filename)
  if not os.path.isdir(dirname):
    os.makedirs(dirname)
  with open(filename, 'w') as f:
    f.write(content)


class AutoninjaTest(trial_dir.TestCase):
  def setUp(self):
    super(AutoninjaTest, self).setUp()
    self.previous_dir = os.getcwd()
    os.chdir(self.root_dir)

  def tearDown(self):
    os.chdir(self.previous_dir)
    super(AutoninjaTest, self).tearDown()

  def test_autoninja(self):
    autoninja.main([])

  def test_autoninja_goma(self):
    with unittest.mock.patch(
        'subprocess.call',
        return_value=0) as mock_call, unittest.mock.patch.dict(
            os.environ, {"GOMA_DIR": os.path.join(self.root_dir, 'goma_dir')}):
      out_dir = os.path.join('out', 'dir')
      write(os.path.join(out_dir, 'args.gn'), 'use_goma=true')
      write(
          os.path.join(
              'goma_dir',
              'gomacc.exe' if sys.platform.startswith('win') else 'gomacc'),
          'content')
      args = autoninja.main(['autoninja.py', '-C', out_dir]).split()
      mock_call.assert_called_once()

    self.assertIn('-j', args)
    parallel_j = int(args[args.index('-j') + 1])
    self.assertGreater(parallel_j, multiprocessing.cpu_count())
    self.assertIn(os.path.join(autoninja.SCRIPT_DIR, 'ninja.py'), args)

  def test_autoninja_reclient(self):
    out_dir = os.path.join('out', 'dir')
    write(os.path.join(out_dir, 'args.gn'), 'use_remoteexec=true')
    write('.gclient', '')
    write('.gclient_entries', 'entries = {"buildtools": "..."}')
    write(os.path.join('buildtools', 'reclient_cfgs', 'reproxy.cfg'), 'RBE_v=2')
    write(os.path.join('buildtools', 'reclient', 'version.txt'), '0.0')

    args = autoninja.main(['autoninja.py', '-C', out_dir]).split()

    self.assertIn('-j', args)
    parallel_j = int(args[args.index('-j') + 1])
    self.assertGreater(parallel_j, multiprocessing.cpu_count())
    self.assertIn(os.path.join(autoninja.SCRIPT_DIR, 'ninja_reclient.py'), args)


if __name__ == '__main__':
  unittest.main()

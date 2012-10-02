#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing
import optparse
import os
import sys

from pylib import android_commands
from pylib import test_options_parser
from pylib import constants


def InstallApk(args):
  options, device = args
  apk_path = os.path.join(os.environ['CHROME_SRC'],
                          'out', options.build_type,
                          'apks', options.apk)
  result = android_commands.AndroidCommands(device=device).ManagedInstall(
      apk_path, False, options.apk_package)
  print '-----  Installed on %s  -----' % device
  print result


def main(argv):
  parser = optparse.OptionParser()
  test_options_parser.AddBuildTypeOption(parser)
  test_options_parser.AddInstallAPKOption(parser)
  options, args = parser.parse_args(argv)

  if len(args) > 1:
    raise Exception('Error: Unknown argument:', args[1:])

  devices = android_commands.GetAttachedDevices()
  if not devices:
    raise Exception('Error: no connected devices')

  pool = multiprocessing.Pool(len(devices))
  # Send a tuple (options, device) per instance of DeploySingleDevice.
  pool.map(InstallApk, zip([options] * len(devices), devices))


if __name__ == '__main__':
  sys.exit(main(sys.argv))

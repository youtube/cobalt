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
from pylib import apk_info
from pylib import constants
from pylib import test_options_parser


def _InstallApk(args):
  apk_path, apk_package, device = args
  result = android_commands.AndroidCommands(device=device).ManagedInstall(
      apk_path, False, apk_package)
  print '-----  Installed on %s  -----' % device
  print result


def main(argv):
  parser = optparse.OptionParser()
  test_options_parser.AddInstallAPKOption(parser)
  options, args = parser.parse_args(argv)
  test_options_parser.ValidateInstallAPKOption(parser, options)
  if len(args) > 1:
    raise Exception('Error: Unknown argument:', args[1:])

  devices = android_commands.GetAttachedDevices()
  if not devices:
    raise Exception('Error: no connected devices')

  if not options.apk_package:
    options.apk_package = apk_info.GetPackageNameForApk(options.apk)

  pool = multiprocessing.Pool(len(devices))
  # Send a tuple (apk_path, apk_package, device) per device.
  pool.map(_InstallApk, zip([options.apk] * len(devices),
                            [options.apk_package] * len(devices),
                            devices))


if __name__ == '__main__':
  sys.exit(main(sys.argv))

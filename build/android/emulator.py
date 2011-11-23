#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides an interface to start and stop Android emulator.

Assumes system environment ANDROID_NDK_ROOT has been set.

  Emulator: The class provides the methods to launch/shutdown the emulator with
            the android virtual device named 'buildbot' .
"""

import logging
import os
import subprocess
import sys

import android_commands


def _GetAvailablePort():
  """Returns an available TCP port for the console."""
  used_ports = []
  emulators = android_commands.GetEmulators()
  for emulator in emulators:
    used_ports.append(emulator.split('-')[1])
  # The port must be an even number between 5554 and 5584.
  for port in range(5554, 5585, 2):
    if str(port) not in used_ports:
      return port


class Emulator(object):
  """Provides the methods to lanuch/shutdown the emulator.

  The emulator has the android virtual device named 'buildbot'.

  The emulator could use any even TCP port between 5554 and 5584 for the
  console communication, and this port will be part of the device name like
  'emulator-5554'. Assume it is always True, as the device name is the id of
  emulator managed in this class.

  Attributes:
    emulator: Path of Android's emulator tool.
    popen: Popen object of the running emulator process.
    device: Device name of this emulator.
  """

  def __init__(self):
    try:
      android_sdk_root = os.environ['ANDROID_SDK_ROOT']
    except KeyError:
      logging.critical('The ANDROID_SDK_ROOT must be set to run the test on '
                       'emulator.')
      raise
    self.emulator = os.path.join(android_sdk_root, 'tools', 'emulator')
    self.popen = None
    self.device = None

  def Launch(self):
    """Launches the emulator and waits for package manager to startup.

    If fails, an exception will be raised.
    """
    port = _GetAvailablePort()
    self.device = "emulator-%d" % port
    self.popen = subprocess.Popen(args=[self.emulator, '-avd', 'buildbot',
                                        '-port', str(port)])
    # This will not return until device's package manager starts up or an
    # exception is raised.
    android_commands.AndroidCommands(self.device, True)

  def Shutdown(self):
    """Shuts down the process started by launch."""
    self.popen.terminate()


def main(argv):
  Emulator().launch()


if __name__ == '__main__':
  main(sys.argv)

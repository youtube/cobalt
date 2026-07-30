#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""This module tests the device monitor."""

import unittest

from device_monitor import DeviceMonitor


# TODO(b/418071419): Add meminfo and SurfaceFlinger tests
class DeviceMonitorTest(unittest.TestCase):

  def testIsPackageRunning(self):

    def run_adb_command(command_args: list):
      del command_args  # unused arg
      return ('1234', None)

    monitor = DeviceMonitor(adb_runner=run_adb_command, time_provider=None)
    self.assertTrue(monitor.is_package_running('some.package.name'))

  def testIsPackageRunningNoPid(self):

    def run_adb_command(command_args: list):
      del command_args  # unused arg
      return ('', None)

    monitor = DeviceMonitor(adb_runner=run_adb_command, time_provider=None)
    self.assertFalse(monitor.is_package_running('some.package.name'))

    def run_adb_command_1(command_args: list):
      del command_args  # unused arg
      return ('No process found', None)

    monitor = DeviceMonitor(adb_runner=run_adb_command_1, time_provider=None)
    self.assertFalse(monitor.is_package_running('some.package.name'))

  def testStopPackageSuccess(self):

    def run_adb_command(command_args: list):
      del command_args  # unused arg
      return ('', None)

    def sleep_provider(time_to_sleep: float):
      del time_to_sleep  # unused
      pass

    monitor = DeviceMonitor(
        adb_runner=run_adb_command, time_provider=sleep_provider)
    self.assertTrue(monitor.stop_package('some.package.name'))

  def testStopPackageFailure(self):
    package_name = 'some.package.name'

    def run_adb_command(command_args: list):
      del command_args  # unused arg
      return ('', 'Test error')

    def sleep_provider(time_to_sleep: float):
      del time_to_sleep  # unused
      pass

    monitor = DeviceMonitor(
        adb_runner=run_adb_command, time_provider=sleep_provider)
    self.assertFalse(monitor.stop_package(package_name))

  def testLaunchCobaltSuccess(self):
    package_name = 'some.package.name'
    activity_name = 'some.activity.name'

    def run_adb_command(command_args: list, shell: bool = False):
      del command_args, shell  # unused args
      return ('', None)

    def sleep_provider(time_to_sleep: float):
      del time_to_sleep  # unused
      pass

    monitor = DeviceMonitor(
        adb_runner=run_adb_command, time_provider=sleep_provider)
    self.assertTrue(
        monitor.launch_cobalt(
            package_name=package_name, activity_name=activity_name, flags=None))

  def testLaunchCobaltFailure(self):
    package_name = 'some.package.name'
    activity_name = 'some.activity.name'

    def run_adb_command(command_args: list, shell: bool = False):
      del command_args, shell  # unused args
      return ('', 'Test error')

    def sleep_provider(time_to_sleep: float):
      del time_to_sleep  # unused
      pass

    monitor = DeviceMonitor(
        adb_runner=run_adb_command, time_provider=sleep_provider)
    self.assertFalse(
        monitor.launch_cobalt(
            package_name=package_name, activity_name=activity_name, flags=None))

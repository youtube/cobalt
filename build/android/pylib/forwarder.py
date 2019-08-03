# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import sys
import time

import android_commands
import cmd_helper
import constants
import ports

from pylib import pexpect


def _MakeBinaryPath(build_type, binary_name):
  return os.path.join(cmd_helper.OutDirectory.get(), build_type, binary_name)


class Forwarder(object):
  """Class to manage port forwards from the device to the host."""

  # Unix Abstract socket path:
  _DEVICE_ADB_CONTROL_PORT = 'chrome_device_forwarder'
  _TIMEOUT_SECS = 30

  _DEVICE_FORWARDER_PATH = constants.TEST_EXECUTABLE_DIR + '/device_forwarder'

  def __init__(self, adb, build_type):
    """Forwards TCP ports on the device back to the host.

    Works like adb forward, but in reverse.

    Args:
      adb: Instance of AndroidCommands for talking to the device.
      build_type: 'Release' or 'Debug'.
    """
    assert build_type in ('Release', 'Debug')
    self._adb = adb
    self._host_to_device_port_map = dict()
    self._device_process = None
    self._host_forwarder_path = _MakeBinaryPath(build_type, 'host_forwarder')
    self._device_forwarder_path = _MakeBinaryPath(
        build_type, 'device_forwarder')

  def Run(self, port_pairs, tool, host_name):
    """Runs the forwarder.

    Args:
      port_pairs: A list of tuples (device_port, host_port) to forward. Note
                 that you can specify 0 as a device_port, in which case a
                 port will by dynamically assigned on the device. You can
                 get the number of the assigned port using the
                 DevicePortForHostPort method.
      tool: Tool class to use to get wrapper, if necessary, for executing the
            forwarder (see valgrind_tools.py).
      host_name: Address to forward to, must be addressable from the
                 host machine. Usually use loopback '127.0.0.1'.

    Raises:
      Exception on failure to forward the port.
    """
    host_adb_control_port = ports.AllocateTestServerPort()
    if not host_adb_control_port:
      raise Exception('Failed to allocate a TCP port in the host machine.')
    self._adb.PushIfNeeded(
        self._device_forwarder_path, Forwarder._DEVICE_FORWARDER_PATH)
    redirection_commands = [
        '%d:%d:%d:%s' % (host_adb_control_port, device, host,
                         host_name) for device, host in port_pairs]
    logging.info('Command format: <ADB port>:<Device port>' +
                 '[:<Forward to port>:<Forward to address>]')
    logging.info('Forwarding using commands: %s', redirection_commands)
    if cmd_helper.RunCmd(
        ['adb', '-s', self._adb._adb.GetSerialNumber(), 'forward',
         'tcp:%s' % host_adb_control_port,
         'localabstract:%s' % Forwarder._DEVICE_ADB_CONTROL_PORT]) != 0:
      raise Exception('Error while running adb forward.')

    (exit_code, output) = self._adb.GetShellCommandStatusAndOutput(
        '%s %s' % (Forwarder._DEVICE_FORWARDER_PATH,
                   Forwarder._DEVICE_ADB_CONTROL_PORT))
    if exit_code != 0:
      raise Exception(
          'Failed to start device forwarder:\n%s' % '\n'.join(output))

    for redirection_command in redirection_commands:
      (exit_code, output) = cmd_helper.GetCmdStatusAndOutput(
          [self._host_forwarder_path, redirection_command])
      if exit_code != 0:
        raise Exception('%s exited with %d:\n%s' % (
            self._host_forwarder_path, exit_code, '\n'.join(output)))
      tokens = output.split(':')
      if len(tokens) != 2:
        raise Exception('Unexpected host forwarder output "%s", ' +
                        'expected "device_port:host_port"' % output)
      device_port = int(tokens[0])
      host_port = int(tokens[1])
      self._host_to_device_port_map[host_port] = device_port
      logging.info('Forwarding device port: %d to host port: %d.', device_port,
                   host_port)

  @staticmethod
  def KillHost(build_type):
    logging.info('Killing host_forwarder.')
    host_forwarder_path = _MakeBinaryPath(build_type, 'host_forwarder')
    (exit_code, output) = cmd_helper.GetCmdStatusAndOutput(
        [host_forwarder_path, 'kill-server'])
    if exit_code != 0:
      (exit_code, output) = cmd_helper.GetCmdStatusAndOutput(
          ['pkill', 'host_forwarder'])
      if exit_code != 0:
        raise Exception('%s exited with %d:\n%s' % (
              host_forwarder_path, exit_code, '\n'.join(output)))

  @staticmethod
  def KillDevice(adb):
    logging.info('Killing device_forwarder.')
    if not adb.FileExistsOnDevice(Forwarder._DEVICE_FORWARDER_PATH):
      return
    (exit_code, output) = adb.GetShellCommandStatusAndOutput(
        '%s kill-server' % Forwarder._DEVICE_FORWARDER_PATH)
    # TODO(pliard): Remove the following call to KillAllBlocking() when we are
    # sure that the old version of device_forwarder (not supporting
    # 'kill-server') is not running on the bots anymore.
    timeout_sec = 5
    processes_killed = adb.KillAllBlocking('device_forwarder', timeout_sec)
    if not processes_killed:
      pids = adb.ExtractPid('device_forwarder')
      if pids:
        raise Exception('Timed out while killing device_forwarder')

  def DevicePortForHostPort(self, host_port):
    """Get the device port that corresponds to a given host port."""
    return self._host_to_device_port_map.get(host_port)

  def Close(self):
    """Terminate the forwarder process."""
    if self._device_process:
      self._device_process.close()
      self._device_process = None

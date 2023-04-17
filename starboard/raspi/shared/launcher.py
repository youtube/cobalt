#
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Raspi implementation of Starboard launcher, based on Fabric."""

from starboard.tools import abstract_launcher
from fabric import Connection
import os
import re
import logging
import threading
from typing import Sequence


# First call returns True, otherwise return false.
def FirstRun():
  glb = globals()
  if 'first_run' not in glb:
    glb['first_run'] = False
    return True
  return False


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Cobalt and tests on Raspi."""

  USERNAME = 'pi'
  CONNECT_TIMEOUT = 5

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    # Platform needs to be renamed while forwarding, other params are ok
    super().__init__(platform, target_name, config, device_id, **kwargs)
    # normalize paths
    self.out_directory = os.path.abspath(self.out_directory)
    self.base_dir = os.path.basename(self.out_directory)
    self.target_host = f'{self.USERNAME}@{self.device_id}'

    self.run_inactive = threading.Event()
    self.run_inactive.set()

    self.shutdown_initiated = threading.Event()

    self.log_targets = kwargs.get('log_targets', True)

  def connection(self):
    return Connection(self.target_host, connect_timeout=self.CONNECT_TIMEOUT)

  def deploy_test(self):
    local_dir = os.path.join(self.out_directory, 'install', self.target_name)
    options = '-avzLhc'
    dest = f'{self.target_host}:~/{self.base_dir}/'
    rsync_command = 'rsync ' + options + ' ' + local_dir + ' ' + dest
    self.connection().local(command=f'{rsync_command}')

  def first_run_clean(self):
    conn = self.connection()
    conn.run('free -mh')
    conn.run('ps -ux')
    conn.run('df -h')
    conn.run(
        'pkill -9 -ef "(cobalt)|(crashpad_handler)|(elf_loader)"', disown=True)

  @classmethod
  def make_flags(cls, params: Sequence):
    flags = ' '.join(params)
    meta_chars = '()[]{}%!^"<>&|'
    meta_re = re.compile('(' + '|'.join(
        re.escape(char) for char in list(meta_chars)) + ')')
    return re.subn(meta_re, r'\\\1', flags)[0]

  def run_test(self):
    target_binary = os.path.join(self.base_dir, self.target_name,
                                 self.target_name)
    test_base_command = target_binary + ' ' + self.make_flags(
        self.target_command_line_params)
    conn = self.connection()
    if self.test_result_xml_path:
      conn.run(f'touch {self.test_result_xml_path}', echo=True)
    conn.run(test_base_command, echo=True)

  def Run(self):
    """Runs launcher's executable on the target raspi.

    Returns:
       Whether or not the run finished successfully.
    """
    if self.log_targets:
      logging.info('-' * 32)
      logging.info('Starting to run target: %s', self.target_name)
      logging.info('=' * 32)

    # Notify other threads that the run is now active
    self.run_inactive.clear()

    self.deploy_test()
    if FirstRun():
      self.first_run_clean()
    self.run_test()

    # Notify other threads that the run is no longer active
    self.run_inactive.set()

    if self.log_targets:
      logging.info('-' * 32)
      logging.info('Finished running target: %s', self.target_name)
      logging.info('=' * 32)

  def Kill(self):
    """Stops the run so that the launcher can be killed."""

    logging.error('\n***Killing Launcher***\n')
    if self.run_inactive.is_set():
      return
    # Initiate the shutdown. This causes the run to abort within one second.
    self.shutdown_initiated.set()
    # Wait up to three seconds for the run to be set to inactive.
    self.run_inactive.wait(3)

  def GetDeviceIp(self):
    """Gets the device IP."""
    return self.device_id

  def GetDeviceOutputPath(self):
    """Writable path where test targets can output files"""
    return '/tmp'

#
# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Linux implementation of Starboard launcher abstraction."""

import os
import signal
import socket
import subprocess
import sys
import time
import traceback

import _env  # pylint: disable=unused-import
from starboard.tools import abstract_launcher

STATUS_CHANGE_TIMEOUT = 15


def GetProcessStatus(pid):
  """Returns process running status given its pid, or empty string if not found.

  Args:
    pid: process id of specified cobalt instance.
  """
  output = subprocess.check_output(
      ["ps -o state= -p {}".format(pid)], shell=True)
  return output


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Cobalt/tools on Linux."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   **kwargs)
    if not self.device_id:
      if socket.has_ipv6:  #  If the device supports IPv6:
        self.device_id = "::1"  #  Use the only IPv6 loopback address
      else:
        self.device_id = socket.gethostbyname("localhost")  # pylint: disable=W6503

    self.executable = self.GetTargetPath()

    env = os.environ.copy()
    env.update(self.env_variables)
    self.full_env = env

    self.proc = None
    self.pid = None

  def Run(self):
    """Runs launcher's executable."""

    self.proc = subprocess.Popen(
        [self.executable] + self.target_command_line_params,
        stdout=self.output_file,
        stderr=self.output_file,
        env=self.full_env,
        close_fds=True)
    self.pid = self.proc.pid
    self.proc.wait()
    return self.proc.returncode

  def Kill(self):
    sys.stderr.write("\n***Killing Launcher***\n")
    if self.pid:
      try:
        self.proc.kill()
      except OSError:
        sys.stderr.write("Error killing launcher with SIGKILL:\n")
        traceback.print_exc(file=sys.stderr)
    else:
      sys.stderr.write("Kill() called before Run(), cannot kill.\n")

  def SupportsSuspendResume(self):
    return True

  def SendResume(self):
    """Sends continue to the launcher's executable."""
    sys.stderr.write("\n***Sending continue signal to executable***\n")
    if self.proc:
      self.proc.send_signal(signal.SIGCONT)
      # Wait for process status change in Linux system.
      self.WaitForProcessStatus("R", STATUS_CHANGE_TIMEOUT)
    else:
      sys.stderr.write("Cannot send continue to executable; it is closed.\n")

  def SendSuspend(self):
    """Sends suspend to the launcher's executable."""
    sys.stderr.write("\n***Sending suspend signal to executable***\n")
    if self.proc:
      self.proc.send_signal(signal.SIGUSR1)
      # Wait for process status change in Linux system.
      self.WaitForProcessStatus("T", STATUS_CHANGE_TIMEOUT)
    else:
      sys.stderr.write("Cannot send suspend to executable; it is closed.\n")

  def WaitForProcessStatus(self, target_status, timeout):
    """Wait for Cobalt to turn to target status within specified timeout limit.

    Args:
      target_status: A character representing application status:
                        R-running;
                        T-stopped/suspended;
                        S-sleep/paused;
      timeout:       Time limit in unit of seconds.
    """
    elapsed_time = 0
    while not GetProcessStatus(pid=self.pid).startswith(target_status):
      if elapsed_time >= timeout:
        return
      else:
        elapsed_time += .005
      time.sleep(.005)

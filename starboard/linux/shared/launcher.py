#!/usr/bin/python
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

import imp
import os
import signal
import socket
import subprocess

module_path = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 os.pardir, os.pardir, "tools", "abstract_launcher.py"))

abstract_launcher = imp.load_source("abstract_launcher", module_path)


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Cobalt/tools on Linux."""

  def __init__(self, platform, target_name, config, device_id, args):
    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   args)
    if not self.device_id:
      if socket.has_ipv6:  #  If the device supports IPv6:
        self.device_id = "::1"  #  Use the only IPv6 loopback address
      else:
        self.device_id = socket.gethostbyname("localhost")

    executable_dir = "{}_{}".format(self.platform, self.config)
    self.executable = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                                   os.pardir, os.pardir,
                                                   os.pardir, "out",
                                                   executable_dir,
                                                   target_name))

    self.pid = None

  def Run(self):
    """Runs launcher's executable."""

    signal.signal(signal.SIGTERM, lambda signum, frame: self.Kill())
    signal.signal(signal.SIGINT, lambda signum, frame: self.Kill())
    proc = subprocess.Popen([self.executable] + self.target_command_line_params)
    self.pid = proc.pid
    proc.wait()

  def Kill(self):
    print "\n***Killing Launcher***\n"
    if self.pid:
      try:
        os.kill(self.pid, signal.SIGTERM)
      except OSError:
        raise OSError("Process already closed.")

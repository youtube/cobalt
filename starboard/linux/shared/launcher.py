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

import importlib
import os
import sys

if "environment" in sys.modules:
  environment = sys.modules["environment"]
else:
  env_path = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir,
                                          os.pardir, "tools"))
  if env_path not in sys.path:
    sys.path.append(env_path)
  environment = importlib.import_module("environment")


import signal
import socket
import subprocess

import starboard.tools.abstract_launcher as abstract_launcher


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Cobalt/tools on Linux."""

  def __init__(self, platform, target_name, config, device_id, args,
               output_file, out_directory):
    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   args, output_file, out_directory)
    if not self.device_id:
      if socket.has_ipv6:  #  If the device supports IPv6:
        self.device_id = "::1"  #  Use the only IPv6 loopback address
      else:
        self.device_id = socket.gethostbyname("localhost")

    self.executable = abstract_launcher.GetDefaultTargetPath(
        platform, config, target_name)

    self.pid = None

  def Run(self):
    """Runs launcher's executable."""
    sys.stderr.write("{}\n".format(self.executable))
    proc = subprocess.Popen([self.executable] + self.target_command_line_params,
                            stdout=self.output_file, stderr=self.output_file)
    self.pid = proc.pid
    proc.wait()
    return proc.returncode

  def Kill(self):
    print "\n***Killing Launcher***\n"
    if self.pid:
      try:
        os.kill(self.pid, signal.SIGTERM)
      except OSError as e:
        sys.stderr.write("Cannot kill launcher.  Process already closed.\n")

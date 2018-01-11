#!/usr/bin/python
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
"""Starboard win-win32 platform configuration for gyp_cobalt."""


from __future__ import print_function

import os
import subprocess
import sys
import traceback

import starboard.tools.abstract_launcher as abstract_launcher

class Launcher(abstract_launcher.AbstractLauncher):

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   **kwargs)
    self.executable = self.GetTargetPath()

  def Run(self):
    sys.stderr.write('\n***Running Launcher***\n')
    """Runs launcher's executable."""
    self.proc = subprocess.Popen(
        [self.executable] + self.target_command_line_params,
        stdout=self.output_file,
        stderr=self.output_file)
    self.pid = self.proc.pid
    self.proc.communicate()
    self.proc.poll()
    return self.proc.returncode

  def Kill(self):
    sys.stderr.write("\n***Killing Launcher***\n")
    if self.pid:
      try:
        self.proc.kill()
      except OSError:
        sys.stderr.write("Error killing launcher with SIGKILL:\n")
        traceback.print_exc(file=sys.stderr)
        # If for some reason Kill() fails then os_.exit(1) will kill the
        # child process without cleanup. Otherwise the process will hang.
        os._exit(1)
    else:
      sys.stderr.write("Kill() called before Run(), cannot kill.\n")

    return

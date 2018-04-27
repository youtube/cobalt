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

import starboard.shared.win32.mini_dump_printer as mini_dump_printer
import starboard.tools.abstract_launcher as abstract_launcher

class Launcher(abstract_launcher.AbstractLauncher):
  def __init__(self, platform, target_name, config, device_id, **kwargs):
    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   **kwargs)
    self.executable_path = self.GetTargetPath()

    self.executable_mini_dump_path = self.executable_path + '.dmp'
    if os.path.exists(self.executable_mini_dump_path):
      self.LogLn('Found previous crash mini dump: deleting.')
      os.remove(self.executable_mini_dump_path)

  def Run(self):
    self.LogLn('\n***Running Launcher***')
    """Runs launcher's executable_path."""
    self.proc = subprocess.Popen(
        [self.executable_path] + self.target_command_line_params,
        stdout=self.output_file,
        stderr=self.output_file)
    self.pid = self.proc.pid
    self.proc.communicate()
    self.proc.poll()
    rtn_code = self.proc.returncode
    self.DetectAndHandleCrashDump()
    self.LogLn('Finished running executable.')
    return rtn_code

  def Kill(self):
    self.LogLn("\n***Killing Launcher***")
    if self.pid:
      try:
        self.proc.kill()
      except OSError:
        self.LogLn("Error killing launcher with SIGKILL:")
        traceback.print_exc(file=sys.stdout)
        # If for some reason Kill() fails then os_.exit(1) will kill the
        # child process without cleanup. Otherwise the process will hang.
        os._exit(1)
    else:
      self.LogLn("Kill() called before Run(), cannot kill.")
    return

  def Log(self, s):
    self.output_file.write(s);

  def LogLn(self, s):
    self.Log(s + '\n');

  def DetectAndHandleCrashDump(self):
    if not os.path.exists(self.executable_mini_dump_path):
      return
    self.LogLn('\n*** Found crash dump! ***\nMinDumpPath:'\
               + self.executable_mini_dump_path)
    mini_dump_printer.PrintMiniDump(
        self.executable_mini_dump_path,
        self.executable_path,
        self.output_file)

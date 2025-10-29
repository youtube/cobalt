#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#
"""tvOS simulator implementation of Starboard launcher abstraction."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import abc
import threading

from starboard.tools import abstract_launcher


class TvOSLauncher(abstract_launcher.AbstractLauncher):
  """Run an application on tvOS simulator."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):

    super().__init__(platform, target_name, config, device_id, **kwargs)

    self.return_code = 1
    self.test_output_stream = None

  @abc.abstractmethod
  def _CleanUp(self):
    pass

  @abc.abstractmethod
  def _LaunchTest(self):
    pass

  def _HasTools(self):
    pass

  def _FilterOutput(self, line):
    return line

  def _ReadResult(self):
    while self.test_output_stream:
      raw_line = self.test_output_stream.readline().decode('utf-8', 'replace')
      if raw_line:
        line = self._FilterOutput(raw_line)
        if not line:
          continue
        self.output_file.write(line)
        # TODO(b/280671902) remove 'test case' entries after gtest upgrade
        # complete.
        if line.find('test cases ran') >= 0 or line.find(
            'test case ran') >= 0 or line.find(
                'test suites ran') >= 0 or line.find('test suite ran') >= 0:
          self.return_code = 0
        elif line.find('FAILED') >= 0:
          self.return_code = 1
      else:
        break
    self.output_file.flush()
    self.test_process.kill()
    self.output_file.close()

  def Run(self):
    if not self._HasTools():
      return 1
    # Default to error return.
    self.return_code = 1
    if self._LaunchTest():
      self.reader_thread = threading.Thread(target=self._ReadResult)
      self.reader_thread.start()
      self.reader_thread.join()
    self._CleanUp()
    return self.return_code

  def Kill(self):
    pass

  def GetDeviceIp(self):
    """Gets the device IP. TODO: Implement."""
    return None

  def GetDeviceOutputPath(self):
    """Writable path where test targets can output files TODO: Implement"""
    return None

# Copyright 2018 Google Inc. All Rights Reserved.
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
"""Specialized Starboard version 4 launcher expressing no preload support."""

import starboard.linux.shared.launcher as launcher


class Launcher(launcher.Launcher):

  def SupportsSuspendResume(self):
    # While technically Starboard version 4 does support suspend/resume, it
    # does not support preload and so we return false here to avoid running
    # tests which assume "SuspendResume" support implies preload support.
    return False

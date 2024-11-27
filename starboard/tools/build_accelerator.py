#
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Defines abstract build accelerators."""

import abc


class BuildAccelerator(object):
  """A base class for all build accelerators."""

  __metaclass__ = abc.ABCMeta

  @abc.abstractmethod
  def GetName(self):
    """Returns name of build accelerator to be called."""
    pass

  @abc.abstractmethod
  def Use(self):
    """Returns boolean of whether the build accelerator should be used."""
    pass

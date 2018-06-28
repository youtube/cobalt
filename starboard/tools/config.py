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
"""Canonical source of valid build config types."""


class Config(object):
  """Enumeration of valid build configs."""
  DEBUG = 'debug'
  DEVEL = 'devel'
  GOLD = 'gold'
  QA = 'qa'

  VALID_CONFIGS = [DEBUG, DEVEL, QA, GOLD]


def GetAll():
  """Returns a list of all valid build configs."""
  return Config.VALID_CONFIGS


def IsValid(name):
  """Returns whether |name| is a valid build config."""
  return name in GetAll()

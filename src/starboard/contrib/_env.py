#
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
#
"""Ask the parent directory to load the project environment."""

from imp import load_source
from os import path
import sys

_ENV = path.abspath(path.join(path.dirname(__file__), path.pardir, '_env.py'))
if not path.exists(_ENV):
  print '%s: Can\'t find repo root.\nMissing parent: %s' % (__file__, _ENV)
  sys.exit(1)
load_source('', _ENV)

#
# Copyright 2016 Google Inc. All Rights Reserved.
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
"""Constants and functions for commonly referenced paths."""

import os

import cobalt
import starboard.tools.paths

STARBOARD_ROOT = starboard.tools.paths.STARBOARD_ROOT

COBALT_ROOT = os.path.abspath(os.path.dirname(cobalt.__file__))

REPOSITORY_ROOT = os.path.abspath(os.path.join(COBALT_ROOT, os.pardir))

THIRD_PARTY_ROOT = os.path.join(REPOSITORY_ROOT, 'third_party')

BUILD_OUTPUT_ROOT = os.path.join(REPOSITORY_ROOT, 'out')


def BuildOutputDirectory(platform, config):
  return os.path.join(BUILD_OUTPUT_ROOT, '%s_%s' %(platform, config))

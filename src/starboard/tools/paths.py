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
"""Constants for commonly referenced paths."""

from os import path

# The absolute path to the Starboard repository.
STARBOARD_ROOT = path.abspath(path.join(path.dirname(__file__), path.pardir))

# The absolute path to the root of the project.
REPOSITORY_ROOT = path.abspath(path.join(STARBOARD_ROOT, path.pardir))

# The absolute path to the directory where GYP base configs live.
BUILD_ROOT = path.join(REPOSITORY_ROOT, 'starboard', 'build')

# The absolute path to the third_party directory.
THIRD_PARTY_ROOT = path.join(REPOSITORY_ROOT, 'third_party')

# The absolute path to the build output directory.
BUILD_OUTPUT_ROOT = path.join(REPOSITORY_ROOT, 'out')


def BuildOutputDirectory(platform, config):
  """Gets the build output directory for the given platform and config."""
  return path.join(BUILD_OUTPUT_ROOT, '%s_%s' % (platform, config))

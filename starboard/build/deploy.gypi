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

# This file is meant to be included into a target to provide a rule
# to deploy a target on a target platform.
#
# The platform_deploy target should be defined in
# starboard/<port_path>/platform_deploy.gyp. This target should perform
# any per-executable logic that is specific to the platform. For example,
# copying per-executable metadata files to the output directory.
#
# To use this, create a gyp target with the following form:
# 'targets': [
#    {
#      'target_name': 'target_deploy',
#      'type': 'none',
#      'dependencies': [
#        'target',
#      ],
#      'variables': {
#        'executable_name': 'target',
#      },
#      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
#    },
#    ...
#
# For example:
#  'targets': [
#    {
#      'target_name': 'nplb',
#      'type': '<(gtest_target_type)',
#      'sources': [...]
#    }
#    {
#      'target_name': 'nplb_deploy',
#      'type': 'none',
#      'dependencies': [
#        'nplb',
#      ],
#      'variables': {
#        'executable_name': 'nplb',
#      },
#      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
#    },

{

  # Flag that will instruct gyp to create a special target in IDEs such as
  # Visual Studio that can be used for launching a target.
  'variables' : {
    'ide_deploy_target': 1,
  },

  # Include the platform specific gypi file include. Note that the
  # expanded value will default to
  # "starboard/build/default_no_deploy.gypi"
  'includes': [ '<(DEPTH)/<(include_path_platform_deploy_gypi)' ],
}

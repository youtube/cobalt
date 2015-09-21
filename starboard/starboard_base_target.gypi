# Copyright 2015 Google Inc. All Rights Reserved.
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

# This file is included in the target defaults from
# cobalt/build/config/base.gypi
{
  'defines': [
    # The canonical way to detect Starboard is #if defined(STARBOARD).
    'STARBOARD',

    # There doesn't appear to be any way to do string concatenation with
    # the / character to specify an include file, so we have to do the
    # concatenation here in GYP.
    # http://stackoverflow.com/questions/29601786/c-preprocessor-building-a-path-string
    'STARBOARD_CONFIGURATION_INCLUDE="starboard/<(target_arch)/configuration.h"',
    'STARBOARD_ATOMIC_INCLUDE="starboard/<(target_arch)/atomic.h"',
  ],
}

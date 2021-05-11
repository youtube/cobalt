# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

# Note, that despite the file extension ".gyp", this file is included by several
# platform variants of linux-x64x11, like a ".gypi" file, since those platforms
# have no need to modify this code.
{
  'includes': [
    # Note that we are 'includes'ing a 'gyp' file, not a 'gypi' file.  The idea
    # is that we just want this file to *be* the parent gyp file.
    '<(DEPTH)/starboard/linux/x64x11/starboard_platform.gyp',
  ],
  'variables': {
      'starboard_platform_sources': [
        '<(DEPTH)/starboard/shared/stub/speech_recognizer_cancel.cc',
        '<(DEPTH)/starboard/shared/stub/speech_recognizer_create.cc',
        '<(DEPTH)/starboard/shared/stub/speech_recognizer_destroy.cc',
        '<(DEPTH)/starboard/shared/stub/speech_recognizer_is_supported.cc',
        '<(DEPTH)/starboard/shared/stub/speech_recognizer_start.cc',
        '<(DEPTH)/starboard/shared/stub/speech_recognizer_stop.cc',
      ],
  },
}

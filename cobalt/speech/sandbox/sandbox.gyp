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

# This is a sample sandbox application for experimenting with the Cobalt
# Speech API.

{
  'targets': [
    {
      'target_name': 'speech_sandbox',
      'type': '<(final_executable_type)',
      'sources': [
        'audio_loader.cc',
        'audio_loader.h',
        'speech_sandbox.cc',
        'speech_sandbox.h',
        'speech_sandbox_main.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/audio/audio.gyp:audio',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/loader/loader.gyp:loader',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
        '<(DEPTH)/googleurl/googleurl.gyp:googleurl',
      ],
    },

    {
      'target_name': 'speech_sandbox_deploy',
      'type': 'none',
      'dependencies': [
        'speech_sandbox',
      ],
      'variables': {
        'executable_name': 'speech_sandbox',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },
  ],
}

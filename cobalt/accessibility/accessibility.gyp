# Copyright 2017 Google Inc. All Rights Reserved.
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
# See the License for the specif

{
  'targets': [
    {
      'target_name': 'accessibility',
      'type': 'static_library',
      'sources': [
        'internal/live_region.cc',
        'internal/live_region.h',
        'internal/text_alternative_helper.cc',
        'internal/text_alternative_helper.h',
        'screen_reader.cc',
        'screen_reader.h',
        'starboard_tts_engine.cc',
        'starboard_tts_engine.h',
        'text_alternative.cc',
        'text_alternative.h',
        'tts_engine.h',
        'tts_logger.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
      ],
    },
  ]
}

// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/audio/audio_buffer.h"

#include "cobalt/audio/audio_helpers.h"
#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace audio {

AudioBuffer::AudioBuffer(float sample_rate, scoped_ptr<ShellAudioBus> audio_bus)
    : sample_rate_(sample_rate), audio_bus_(audio_bus.Pass()) {
  DCHECK_GT(sample_rate_, 0);
  DCHECK_GT(length(), 0);
  DCHECK_GT(number_of_channels(), 0);
}

}  // namespace audio
}  // namespace cobalt

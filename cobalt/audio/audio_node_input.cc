/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/audio/audio_node_input.h"

#include "base/logging.h"
#include "cobalt/audio/audio_node.h"
#include "cobalt/audio/audio_node_output.h"

namespace cobalt {
namespace audio {

AudioNodeInput::AudioNodeInput(AudioNode* node) : owner_node_(node) {}

void AudioNodeInput::Connect(AudioNodeOutput* output) {
  DCHECK(output);

  // There can only be one connection between a given output of one specific
  // node and a given input of another specific node. Multiple connections with
  // the same termini are ignored.
  if (outputs_.find(output) != outputs_.end()) {
    return;
  }

  output->AddInput(this);
  outputs_.insert(output);
}

void AudioNodeInput::Disconnect(AudioNodeOutput* output) {
  DCHECK(output);

  if (outputs_.find(output) == outputs_.end()) {
    NOTREACHED();
    return;
  }

  outputs_.erase(output);
  output->RemoveInput(this);
}

void AudioNodeInput::DisconnectAll() {
  while (!outputs_.empty()) {
    AudioNodeOutput* output = *outputs_.begin();
    Disconnect(output);
  }
}

}  // namespace audio
}  // namespace cobalt

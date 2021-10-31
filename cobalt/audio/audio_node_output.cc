// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/audio/audio_node_output.h"

#include "base/logging.h"
#include "cobalt/audio/audio_context.h"
#include "cobalt/audio/audio_node.h"
#include "cobalt/audio/audio_node_input.h"

namespace cobalt {
namespace audio {

typedef media::AudioBus AudioBus;

AudioNodeOutput::~AudioNodeOutput() {
  owner_node_->audio_lock()->AssertLocked();

  DCHECK(inputs_.empty());
}

void AudioNodeOutput::AddInput(AudioNodeInput* input) {
  owner_node_->audio_lock()->AssertLocked();

  DCHECK(input);

  inputs_.insert(input);
}

void AudioNodeOutput::RemoveInput(AudioNodeInput* input) {
  owner_node_->audio_lock()->AssertLocked();

  DCHECK(input);

  inputs_.erase(input);
}

void AudioNodeOutput::DisconnectAll() {
  owner_node_->audio_lock()->AssertLocked();

  while (!inputs_.empty()) {
    AudioNodeInput* input = *inputs_.begin();
    input->Disconnect(this);
  }
}

std::unique_ptr<AudioBus> AudioNodeOutput::PassAudioBusFromSource(
    int32 number_of_frames, SampleType sample_type, bool* finished) {
  // This is called by Audio thread.
  owner_node_->audio_lock()->AssertLocked();

  // Pull audio buffer from its owner node.
  return owner_node_->PassAudioBusFromSource(number_of_frames, sample_type,
                                             finished);
}

}  // namespace audio
}  // namespace cobalt

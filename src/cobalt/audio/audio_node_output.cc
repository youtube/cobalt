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

#include "cobalt/audio/audio_node_output.h"

#include "base/logging.h"
#include "cobalt/audio/audio_context.h"
#include "cobalt/audio/audio_node.h"
#include "cobalt/audio/audio_node_input.h"

namespace cobalt {
namespace audio {

#if defined(COBALT_MEDIA_SOURCE_2016)
typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

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

scoped_ptr<ShellAudioBus> AudioNodeOutput::PassAudioBusFromSource(
    int32 number_of_frames, SampleType sample_type, bool* finished) {
  // This is called by Audio thread.
  owner_node_->audio_lock()->AssertLocked();

  // Pull audio buffer from its owner node.
  return owner_node_
      ->PassAudioBusFromSource(number_of_frames, sample_type, finished)
      .Pass();
}

}  // namespace audio
}  // namespace cobalt

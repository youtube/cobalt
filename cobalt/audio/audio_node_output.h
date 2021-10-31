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

#ifndef COBALT_AUDIO_AUDIO_NODE_OUTPUT_H_
#define COBALT_AUDIO_AUDIO_NODE_OUTPUT_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/audio/audio_buffer.h"
#include "cobalt/audio/audio_helpers.h"
#include "cobalt/media/base/audio_bus.h"

namespace cobalt {
namespace audio {

class AudioNode;
class AudioNodeInput;

// This represents the output coming out of the AudioNode.
// It may be connected to one or more AudioNodeInputs.
class AudioNodeOutput : public base::RefCountedThreadSafe<AudioNodeOutput> {
  typedef media::AudioBus AudioBus;

 public:
  explicit AudioNodeOutput(AudioNode* owner_node) : owner_node_(owner_node) {}
  ~AudioNodeOutput();

  void AddInput(AudioNodeInput* input);
  void RemoveInput(AudioNodeInput* input);

  void DisconnectAll();

  std::unique_ptr<AudioBus> PassAudioBusFromSource(int32 number_of_frames,
                                                   SampleType sample_type,
                                                   bool* finished);

 private:
  AudioNode* const owner_node_;
  std::set<AudioNodeInput*> inputs_;
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_NODE_OUTPUT_H_

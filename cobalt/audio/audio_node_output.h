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

#ifndef COBALT_AUDIO_AUDIO_NODE_OUTPUT_H_
#define COBALT_AUDIO_AUDIO_NODE_OUTPUT_H_

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/audio/audio_buffer.h"
#include "cobalt/audio/audio_helpers.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace audio {

class AudioNode;
class AudioNodeInput;

// This represents the output coming out of the AudioNode.
// It may be connected to one or more AudioNodeInputs.
class AudioNodeOutput : public base::RefCountedThreadSafe<AudioNodeOutput> {
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

 public:
  explicit AudioNodeOutput(AudioNode* owner_node) : owner_node_(owner_node) {}
  ~AudioNodeOutput();

  void AddInput(AudioNodeInput* input);
  void RemoveInput(AudioNodeInput* input);

  void DisconnectAll();

  scoped_ptr<ShellAudioBus> PassAudioBusFromSource(int32 number_of_frames,
                                                   SampleType sample_type,
                                                   bool* finished);

 private:
  AudioNode* const owner_node_;
  std::set<AudioNodeInput*> inputs_;
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_NODE_OUTPUT_H_

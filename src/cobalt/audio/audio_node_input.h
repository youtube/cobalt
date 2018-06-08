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

#ifndef COBALT_AUDIO_AUDIO_NODE_INPUT_H_
#define COBALT_AUDIO_AUDIO_NODE_INPUT_H_

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/audio/audio_buffer.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace audio {

class AudioNode;
class AudioNodeOutput;

// This represents the input feeding into the AudioNode.
// It may be connected to one or more AudioNodeOutputs.
// A connection to the input means connecting an output of an AudioNode
// (AudioNodeOutput) to an input of an AudioNode (AudioNodeInput).
// Each AudioNodeInput has a specific number of channels at any given time. This
// number can change depending on the connection(s) made to the input. If the
// input has no connections, then it has one channel which is silent.
class AudioNodeInput : public base::RefCountedThreadSafe<AudioNodeInput> {
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

 public:
  explicit AudioNodeInput(AudioNode* owner_node) : owner_node_(owner_node) {}
  ~AudioNodeInput();

  void Connect(AudioNodeOutput* output);
  void Disconnect(AudioNodeOutput* output);

  void DisconnectAll();

  // For each input, an AudioNode performs a mixing of all connections to that
  // input. FillAudioBus() performs that action. In the case of multiple
  // connections, it sums the result into |audio_bus|.
  void FillAudioBus(ShellAudioBus* audio_bus, bool* silence,
                    bool* all_finished);

 private:
  AudioNode* const owner_node_;
  std::set<AudioNodeOutput*> outputs_;
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_NODE_INPUT_H_

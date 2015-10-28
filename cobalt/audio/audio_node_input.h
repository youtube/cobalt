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

#ifndef AUDIO_AUDIO_NODE_INPUT_H_
#define AUDIO_AUDIO_NODE_INPUT_H_

#include <set>

#include "base/memory/ref_counted.h"

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
 public:
  explicit AudioNodeInput(AudioNode* node);

  void Connect(AudioNodeOutput* output);
  void Disconnect(AudioNodeOutput* output);

  void DisconnectAll();

 private:
  AudioNode* owner_node_;

  std::set<AudioNodeOutput*> outputs_;
};

}  // namespace audio
}  // namespace cobalt

#endif  // AUDIO_AUDIO_NODE_INPUT_H_

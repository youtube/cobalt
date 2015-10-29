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

#ifndef AUDIO_AUDIO_NODE_OUTPUT_H_
#define AUDIO_AUDIO_NODE_OUTPUT_H_

#include <set>

#include "base/memory/ref_counted.h"

namespace cobalt {
namespace audio {

class AudioNodeInput;

// This represents the output coming out of the AudioNode.
// It may be connected to one or more AudioNodeInputs.
class AudioNodeOutput : public base::RefCountedThreadSafe<AudioNodeOutput> {
 public:
  void AddInput(AudioNodeInput* input);
  void RemoveInput(AudioNodeInput* input);

  void DisconnectAll();

 private:
  std::set<AudioNodeInput*> inputs_;
};

}  // namespace audio
}  // namespace cobalt

#endif  // AUDIO_AUDIO_NODE_OUTPUT_H_

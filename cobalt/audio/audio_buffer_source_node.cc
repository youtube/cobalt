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

#include "cobalt/audio/audio_buffer_source_node.h"

#include "cobalt/audio/audio_node_output.h"

namespace cobalt {
namespace audio {

// numberOfInputs  : 0
// numberOfOutputs : 1
AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* context)
    : AudioNode(context), start_time_(0), end_time_(0) {
  AddOutput(new AudioNodeOutput());
}

void AudioBufferSourceNode::Start(double when, double offset) {
  Start(when, offset, 0);
}

// TODO(***REMOVED***): Fully implement start and stop method.
void AudioBufferSourceNode::Start(double when, double /*offset*/,
                                  double /*duration*/) {
  start_time_ = when;
}

void AudioBufferSourceNode::Stop(double when) { end_time_ = when; }

}  // namespace audio
}  // namespace cobalt

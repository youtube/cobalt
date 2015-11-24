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

#include "cobalt/audio/audio_destination_node.h"

namespace cobalt {
namespace audio {

namespace {
// An AudioDestinationNode representing the audio hardware end-point (the
// normal case) can potentially output more than 2 channels of audio if the
// audio hardware is multi-channel. Max channel count is the maximum number of
// channels that this hardware is capable of supporting. If this value is 0,
// then this indicates that channel count may not be changed.
// TODO(***REMOVED***): Get the actual maximum channel count that can be supported from
// hardware. We only support up to two stereo channels for now.
const uint32 kMaxChannelCount = 2;
}  // namespace

// numberOfInputs  : 1
// numberOfOutputs : 0
AudioDestinationNode::AudioDestinationNode(AudioContext* context)
    : AudioNode(context),
      max_channel_count_(kMaxChannelCount),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          audio_device_(static_cast<int>(channel_count(NULL)), this)) {
  AddInput(new AudioNodeInput(this));
}

void AudioDestinationNode::FillAudioBus(ShellAudioBus* audio_bus,
                                        bool* silence) {
  // Destination node only has one input.
  DCHECK_EQ(number_of_inputs(), 1u);
  Input(0)->FillAudioBus(audio_bus, silence);
}

}  // namespace audio
}  // namespace cobalt

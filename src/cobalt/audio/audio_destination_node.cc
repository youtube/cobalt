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

#include "cobalt/audio/audio_destination_node.h"

#include "cobalt/audio/audio_context.h"

namespace cobalt {
namespace audio {

namespace {
// An AudioDestinationNode representing the audio hardware end-point (the
// normal case) can potentially output more than 2 channels of audio if the
// audio hardware is multi-channel. Max channel count is the maximum number of
// channels that this hardware is capable of supporting. If this value is 0,
// then this indicates that channel count may not be changed.
// TODO: Get the actual maximum channel count that can be supported from
// hardware. We only support up to two stereo channels for now.
const uint32 kMaxChannelCount = 2;
}  // namespace

// numberOfInputs  : 1
// numberOfOutputs : 0
AudioDestinationNode::AudioDestinationNode(AudioContext* context)
    : AudioNode(context),
      message_loop_(MessageLoop::current()),
      max_channel_count_(kMaxChannelCount) {
  AudioLock::AutoLock lock(audio_lock());

  AddInput(new AudioNodeInput(this));
}

AudioDestinationNode::~AudioDestinationNode() {
  // Clear the audio device before lock to avoid race condition of destroying
  // AudioDestinationNode and calling FillAudioBus() from the Audio thread.
  audio_device_.reset();

  AudioLock::AutoLock lock(audio_lock());

  DCHECK_EQ(number_of_outputs(), 0u);
  RemoveAllInputs();
}

void AudioDestinationNode::OnInputNodeConnected() {
  audio_lock()->AssertLocked();

  if (!audio_device_) {
    audio_device_.reset(
        new AudioDevice(static_cast<int>(channel_count(NULL)), this));
  }
  audio_device_to_delete_ = NULL;
}

void AudioDestinationNode::FillAudioBus(bool all_consumed,
                                        ShellAudioBus* audio_bus,
                                        bool* silence) {
  // This is called on Audio thread.
  AudioLock::AutoLock lock(audio_lock());

  // Destination node only has one input.
  DCHECK_EQ(number_of_inputs(), 1u);
  bool all_finished = true;
  Input(0)->FillAudioBus(audio_bus, silence, &all_finished);
  if (all_consumed && all_finished) {
    audio_device_to_delete_ = audio_device_.get();
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&AudioDestinationNode::DestroyAudioDevice,
                              base::Unretained(this)));
  }
}

void AudioDestinationNode::DestroyAudioDevice() {
  if (audio_device_ == audio_device_to_delete_) {
    audio_device_.reset();
  }
}

}  // namespace audio
}  // namespace cobalt

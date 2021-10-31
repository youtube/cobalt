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

#ifndef COBALT_AUDIO_AUDIO_DESTINATION_NODE_H_
#define COBALT_AUDIO_AUDIO_DESTINATION_NODE_H_

#include <memory>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "cobalt/audio/audio_device.h"
#include "cobalt/audio/audio_helpers.h"
#include "cobalt/audio/audio_node.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace audio {

// This is an AudioNode representing the final audio destination and is what
// the user will ultimately hear. If can often be considered as an audio output
// device which is connected to speakers. All rendered audio to be heard will be
// routed to this node, a "terminal" node in the AudioContext's routing graph.
// There is only a single AudioDestinationNode per AudioContext, provided
// through the destination attribute of AudioContext.
//   https://www.w3.org/TR/webaudio/#AudioDestinationNode
class AudioDestinationNode : public AudioNode,
                             public AudioDevice::RenderCallback {
  typedef media::AudioBus AudioBus;

 public:
  AudioDestinationNode(script::EnvironmentSettings* settings,
                       AudioContext* context);

  // Web API: AudioDestinationNode
  //
  // The maximum number of channels that the channel count attribute can be set
  // to.
  uint32 max_channel_count() const { return max_channel_count_; }

  // From AudioNode.
  void OnInputNodeConnected() override;
  std::unique_ptr<AudioBus> PassAudioBusFromSource(int32 number_of_frames,
                                                   SampleType sample_type,
                                                   bool* finished) override {
    NOTREACHED();
    return std::unique_ptr<AudioBus>();
  }

  // From AudioDevice::RenderCallback.
  void FillAudioBus(bool all_consumed, AudioBus* audio_bus,
                    bool* silence) override;

  DEFINE_WRAPPABLE_TYPE(AudioDestinationNode);

 protected:
  ~AudioDestinationNode() override;

 private:
  void DestroyAudioDevice();

  base::MessageLoop* message_loop_;
  uint32 max_channel_count_;

  std::unique_ptr<AudioDevice> audio_device_;
  std::atomic_bool delete_audio_device_ = {false};

  DISALLOW_COPY_AND_ASSIGN(AudioDestinationNode);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_DESTINATION_NODE_H_

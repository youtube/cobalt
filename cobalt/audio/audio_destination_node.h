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

#ifndef COBALT_AUDIO_AUDIO_DESTINATION_NODE_H_
#define COBALT_AUDIO_AUDIO_DESTINATION_NODE_H_

#include <vector>

#include "cobalt/audio/audio_device.h"
#include "cobalt/audio/audio_helpers.h"
#include "cobalt/audio/audio_node.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

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
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

 public:
  explicit AudioDestinationNode(AudioContext* context);

  // Web API: AudioDestinationNode
  //
  // The maximum number of channels that the channel count attribute can be set
  // to.
  uint32 max_channel_count() const { return max_channel_count_; }

  // From AudioNode.
  void OnInputNodeConnected() override;
  scoped_ptr<ShellAudioBus> PassAudioBusFromSource(int32 /*number_of_frames*/,
                                                   SampleType) override {
    NOTREACHED();
    return scoped_ptr<ShellAudioBus>();
  }

  // From AudioDevice::RenderCallback.
  void FillAudioBus(ShellAudioBus* audio_bus, bool* silence) override;

  DEFINE_WRAPPABLE_TYPE(AudioDestinationNode);

 protected:
  ~AudioDestinationNode() override;

 private:
  uint32 max_channel_count_;

  scoped_ptr<AudioDevice> audio_device_;

  DISALLOW_COPY_AND_ASSIGN(AudioDestinationNode);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_AUDIO_DESTINATION_NODE_H_

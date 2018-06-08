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

#ifndef COBALT_AUDIO_AUDIO_NODE_H_
#define COBALT_AUDIO_AUDIO_NODE_H_

#include <string>
#include <vector>

#include "cobalt/audio/audio_helpers.h"
#include "cobalt/audio/audio_node_channel_count_mode.h"
#include "cobalt/audio/audio_node_channel_interpretation.h"
#include "cobalt/audio/audio_node_input.h"
#include "cobalt/audio/audio_node_output.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/event_target.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_audio_bus.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/shell_audio_bus.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace audio {

class AudioLock;
class AudioContext;

// AudioNodes are the building blocks of an AudioContext. This interface
// represents audio sources, the audio destination, and intermediate processing
// modules. These modules can be connected together to form processing graphs
// for rendering audio to the audio hardware.  Each node can have inputs and/or
// outputs. A source node has no inputs and a single output. An
// AudioDestinationNode has one input and no outputs and represents the final
// destination to the audio hardware. Most processing nodes such as filters will
// have one input and one output. Each type of AudioNode differs in the details
// of how it processes or synthesizes audio. But, in general, AudioNodes will
// process its inputs (if it has any), and generate audio for its outputs
// (if it has any).
//   https://www.w3.org/TR/webaudio/#AudioNode-section
class AudioNode : public dom::EventTarget {
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

 public:
  explicit AudioNode(AudioContext* context);

  // Web API: AudioNode
  //
  // The AudioContext which owns this AudioNode.
  scoped_refptr<AudioContext> context() const;

  // The number of inputs feeding into the AudioNode. For source nodes, this
  // will be 0.
  uint32 number_of_inputs() const {
    return static_cast<uint32>(inputs_.size());
  }
  // The number of outputs coming out of the AudioNode. This will be 0 for an
  // AudioDestinationNode.
  uint32 number_of_outputs() const {
    return static_cast<uint32>(outputs_.size());
  }

  // The number of channels used when up-mixing and down-mixing connections to
  // any inputs to the node. The default value is 2 except for specific nodes
  // where its value is specially determined. This attributes has no effect for
  // nodes with no inputs.
  uint32 channel_count(script::ExceptionState* /* unused */) const {
    return channel_count_;
  }
  void set_channel_count(uint32 channel_count,
                         script::ExceptionState* exception_state);

  // Determines how channels will be counted when up-mixing and down-mixing
  // connections to any inputs to the node. This attributes has no effect for
  // nodes with no inputs.
  const AudioNodeChannelCountMode& channel_count_mode() const {
    return channel_count_mode_;
  }
  void set_channel_count_mode(
      const AudioNodeChannelCountMode& channel_count_mode);

  // Determines how individual channels will be treated when up-mixing and
  // down-mixing connections to any inputs to the node. This attribute has no
  // effect for nodes with no inputs.
  const AudioNodeChannelInterpretation& channel_interpretation() const {
    return channel_interpretation_;
  }
  void set_channel_interpretation(
      const AudioNodeChannelInterpretation& channel_interpretation);

  // Connects the AudioNode to another AudioNode.
  void Connect(const scoped_refptr<AudioNode>& destination, uint32 output,
               uint32 input, script::ExceptionState* exception_state);
  // Disconnects an AudioNode's output.
  void Disconnect(uint32 output, script::ExceptionState* exception_state);

  // Called when a new input node has been connected.
  virtual void OnInputNodeConnected() {}

  virtual scoped_ptr<ShellAudioBus> PassAudioBusFromSource(
      int32 number_of_frames, SampleType sample_type, bool* finished) = 0;

  AudioLock* audio_lock() const { return audio_lock_.get(); }

  DEFINE_WRAPPABLE_TYPE(AudioNode);

 protected:
  ~AudioNode() override;

  void AddInput(const scoped_refptr<AudioNodeInput>& input);
  void AddOutput(const scoped_refptr<AudioNodeOutput>& output);

  void RemoveAllInputs();
  void RemoveAllOutputs();

  AudioNodeInput* Input(int32 index) const;
  AudioNodeOutput* Output(int32 index) const;

 private:
  typedef std::vector<scoped_refptr<AudioNodeInput> > AudioNodeInputVector;
  typedef std::vector<scoped_refptr<AudioNodeOutput> > AudioNodeOutputVector;

  // From EventTarget.
  std::string GetDebugName() override { return "AudioNode"; }

  AudioNodeInputVector inputs_;
  AudioNodeOutputVector outputs_;

  AudioContext* audio_context_;

  scoped_refptr<AudioLock> audio_lock_;

  // Channel up-mixing and down-mixing rules for all inputs.
  uint32 channel_count_;
  AudioNodeChannelCountMode channel_count_mode_;
  AudioNodeChannelInterpretation channel_interpretation_;

  DISALLOW_COPY_AND_ASSIGN(AudioNode);
};

}  // namespace audio
}  // namespace cobalt
#endif  // COBALT_AUDIO_AUDIO_NODE_H_

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

#ifndef AUDIO_AUDIO_NODE_H_
#define AUDIO_AUDIO_NODE_H_

#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace audio {

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
//   http://www.w3.org/TR/webaudio/#AudioNode-section
class AudioNode : public dom::EventTarget {
 public:
  enum ChannelCountMode {
    kMax,
    kClampedMax,
    kExplicit,
  };

  enum ChannelInterpretation {
    kSpeakers,
    kDiscrete,
  };

  explicit AudioNode(AudioContext* context);

  // Web API: AudioNode
  //
  // The AudioContext which owns this AudioNode.
  AudioContext* context() const { return audio_context_; }

  // The number of inputs feeding into the AudioNode. For source nodes, this
  // will be 0.
  uint32 number_of_inputs() const {
    // TODO(***REMOVED***): Implement this.
    return 0;
  }
  // The number of outputs coming out of the AudioNode. This will be 0 for an
  // AudioDestinationNode.
  uint32 number_of_outputs() const {
    // TODO(***REMOVED***): Implement this.
    return 0;
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
  const ChannelCountMode& channel_count_mode() const {
    return channel_count_mode_;
  }
  void set_channel_count_mode(const ChannelCountMode& channel_count_mode) {
    channel_count_mode_ = channel_count_mode;
  }

  // Determines how individual channels will be treated when up-mixing and
  // down-mixing connections to any inputs to the node. This attribute has no
  // effect for nodes with no inputs.
  const ChannelInterpretation& channel_interpretation() const {
    return channel_interpretation_;
  }
  void set_channel_interpretation(
      const ChannelInterpretation& channel_interpretation) {
    channel_interpretation_ = channel_interpretation;
  }

  // Connects the AudioNode to another AudioNode.
  void Connect(const scoped_refptr<AudioNode>& destination, uint32 output,
               uint32 input, script::ExceptionState* exception_state);
  // Disconnects an AudioNode's output.
  void Disconnect(uint32 output, script::ExceptionState* exception_state);

  DEFINE_WRAPPABLE_TYPE(AudioNode);

 private:
  AudioContext* audio_context_;

  // Channel up-mixing and down-mixing rules for all inputs.
  uint32 channel_count_;
  ChannelCountMode channel_count_mode_;
  ChannelInterpretation channel_interpretation_;

  DISALLOW_COPY_AND_ASSIGN(AudioNode);
};

}  // namespace audio
}  // namespace cobalt
#endif  // AUDIO_AUDIO_NODE_H_

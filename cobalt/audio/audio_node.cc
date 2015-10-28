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

#include "cobalt/audio/audio_node.h"

#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace audio {

AudioNode::AudioNode(AudioContext* context)
    : audio_context_(context),
      channel_count_(2),
      channel_count_mode_(AudioNode::kMax),
      channel_interpretation_(AudioNode::kSpeakers) {}

AudioNode::~AudioNode() {
  while (!inputs_.empty()) {
    AudioNodeInput* input = inputs_.back();
    input->DisconnectAll();
    inputs_.pop_back();
  }

  while (!outputs_.empty()) {
    AudioNodeOutput* output = outputs_.back();
    output->DisconnectAll();
    outputs_.pop_back();
  }
}

void AudioNode::set_channel_count(uint32 channel_count,
                                  script::ExceptionState* exception_state) {
  // If this value is set to zero, the implementation MUST throw a
  // NOT_SUPPORTED_ERR exception.
  if (channel_count == 0) {
    dom::DOMException::Raise(dom::DOMException::kNotSupportedErr,
                             exception_state);
    return;
  }

  channel_count_ = channel_count;
}

void AudioNode::Connect(const scoped_refptr<AudioNode>& destination,
                        uint32 output, uint32 input,
                        script::ExceptionState* exception_state) {
  // The destination parameter is the AudioNode to connect to.
  if (!destination) {
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, exception_state);
    return;
  }
  // The output parameter is an index describing which output of the AudioNode
  // from which to connect. If this paremeter is out-of-bound, an INDEX_SIZE_ERR
  // exception MUST be thrown.
  if (output >= number_of_outputs()) {
    dom::DOMException::Raise(dom::DOMException::kIndexSizeErr, exception_state);
    return;
  }
  // The input parameter is an index describing which input of the destination
  // AudioNode to connect to. If this parameter is out-of-bound, an
  // INDEX_SIZE_ERR exception MUST be thrown.
  if (input >= destination->number_of_inputs()) {
    dom::DOMException::Raise(dom::DOMException::kIndexSizeErr, exception_state);
    return;
  }

  // TODO(***REMOVED***): Detect if there is a cycle when connecting an AudioNode to
  // another AudioNode. A cycle is allowed only if there is at least one
  // DelayNode in the cycle or a NOT_SUPPORTED_ERR exception MUST be thrown.
  AudioNodeInput* input_node = destination->inputs_[input];
  AudioNodeOutput* output_node = outputs_[output];

  DCHECK(input_node);
  DCHECK(output_node);

  input_node->Connect(output_node);
}

void AudioNode::Disconnect(uint32 output,
                           script::ExceptionState* exception_state) {
  // The output parameter is an index describing which output of the AudioNode
  // to disconnect. If the output parameter is out-of-bounds, an INDEX_SIZE_ERR
  // exception MUST be thrown.
  if (output >= number_of_outputs()) {
    dom::DOMException::Raise(dom::DOMException::kIndexSizeErr, exception_state);
    return;
  }

  AudioNodeOutput* output_node = outputs_[output].get();

  DCHECK(output_node);
  output_node->DisconnectAll();
}

void AudioNode::AddInput(const scoped_refptr<AudioNodeInput>& input) {
  DCHECK(input);

  inputs_.push_back(input);
}

void AudioNode::AddOutput(const scoped_refptr<AudioNodeOutput>& output) {
  DCHECK(output);

  outputs_.push_back(output);
}

}  // namespace audio
}  // namespace cobalt

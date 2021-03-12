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

#include "cobalt/audio/audio_node.h"

#include "cobalt/audio/audio_context.h"
#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace audio {

AudioNode::AudioNode(script::EnvironmentSettings* settings,
                     AudioContext* context)
    : EventTarget(settings),
      audio_context_(context),
      audio_lock_(context->audio_lock()),
      channel_count_(2),
      channel_count_mode_(kAudioNodeChannelCountModeMax),
      channel_interpretation_(kAudioNodeChannelInterpretationSpeakers) {}

AudioNode::~AudioNode() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AudioLock::AutoLock lock(audio_lock());

  RemoveAllInputs();
  RemoveAllOutputs();
}

scoped_refptr<AudioContext> AudioNode::context() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return audio_context_;
}

void AudioNode::set_channel_count(uint32 channel_count,
                                  script::ExceptionState* exception_state) {
  AudioLock::AutoLock lock(audio_lock());

  // If this value is set to zero, the implementation MUST throw a
  // NOT_SUPPORTED_ERR exception.
  if (channel_count == 0) {
    dom::DOMException::Raise(dom::DOMException::kNotSupportedErr,
                             "Audio node channel count must be non-zero.",
                             exception_state);
    return;
  }

  // TODO: Check if this AudioNode is destination when setting the
  // channel count. If it is destination, channel count may be set to any
  // non-zero value less than or equal to max channel count of the destination.
  // An INDEX_SIZE_ERR exception MUST be thrown if this value is not within the
  // valid range.
  channel_count_ = channel_count;
}

void AudioNode::set_channel_count_mode(
    const AudioNodeChannelCountMode& channel_count_mode) {
  AudioLock::AutoLock lock(audio_lock());

  channel_count_mode_ = channel_count_mode;
}

void AudioNode::set_channel_interpretation(
    const AudioNodeChannelInterpretation& channel_interpretation) {
  AudioLock::AutoLock lock(audio_lock());

  channel_interpretation_ = channel_interpretation;
}

void AudioNode::Connect(const scoped_refptr<AudioNode>& destination,
                        uint32 output, uint32 input,
                        script::ExceptionState* exception_state) {
  AudioLock::AutoLock lock(audio_lock());

  // The destination parameter is the AudioNode to connect to.
  if (!destination) {
    dom::DOMException::Raise(dom::DOMException::kSyntaxErr, exception_state);
    return;
  }
  // The output parameter is an index describing which output of the AudioNode
  // from which to connect. If this parameter is out-of-bound, an INDEX_SIZE_ERR
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

  // TODO: Detect if there is a cycle when connecting an AudioNode to
  // another AudioNode. A cycle is allowed only if there is at least one
  // DelayNode in the cycle or a NOT_SUPPORTED_ERR exception MUST be thrown.
  AudioNodeInput* input_node = destination->inputs_[input].get();
  AudioNodeOutput* output_node = outputs_[output].get();

  DCHECK(input_node);
  DCHECK(output_node);

  input_node->Connect(output_node);
}

void AudioNode::Disconnect(uint32 output,
                           script::ExceptionState* exception_state) {
  AudioLock::AutoLock lock(audio_lock());

  // The output parameter is an index describing which output of the AudioNode
  // to disconnect. If the output parameter is out-of-bounds, an INDEX_SIZE_ERR
  // exception MUST be thrown.
  if (output >= number_of_outputs()) {
    dom::DOMException::Raise(dom::DOMException::kIndexSizeErr, exception_state);
    return;
  }

  scoped_refptr<AudioNodeOutput> output_node = outputs_[output];

  DCHECK(output_node);
  output_node->DisconnectAll();
}

void AudioNode::AddInput(const scoped_refptr<AudioNodeInput>& input) {
  audio_lock()->AssertLocked();

  DCHECK(input);

  inputs_.push_back(input);
}

void AudioNode::AddOutput(const scoped_refptr<AudioNodeOutput>& output) {
  audio_lock()->AssertLocked();

  DCHECK(output);

  outputs_.push_back(output);
}

void AudioNode::RemoveAllInputs() {
  audio_lock()->AssertLocked();

  while (!inputs_.empty()) {
    scoped_refptr<AudioNodeInput> input = inputs_.back();
    input->DisconnectAll();
    inputs_.pop_back();
  }
}

void AudioNode::RemoveAllOutputs() {
  audio_lock()->AssertLocked();

  while (!outputs_.empty()) {
    scoped_refptr<AudioNodeOutput> output = outputs_.back();
    output->DisconnectAll();
    outputs_.pop_back();
  }
}

AudioNodeInput* AudioNode::Input(int32 index) const {
  audio_lock()->AssertLocked();

  size_t input_index = static_cast<size_t>(index);
  if (input_index < inputs_.size()) {
    return inputs_[input_index].get();
  }
  return NULL;
}

AudioNodeOutput* AudioNode::Output(int32 index) const {
  audio_lock()->AssertLocked();

  size_t output_index = static_cast<size_t>(index);
  if (output_index < outputs_.size()) {
    return outputs_[output_index].get();
  }
  return NULL;
}

void AudioNode::TraceMembers(script::Tracer* tracer) {
  EventTarget::TraceMembers(tracer);

  tracer->Trace(audio_context_);
}

}  // namespace audio
}  // namespace cobalt

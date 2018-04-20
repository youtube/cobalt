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

#include "cobalt/audio/audio_buffer_source_node.h"

#include <algorithm>

#include "cobalt/audio/audio_context.h"
#include "cobalt/audio/audio_node_output.h"

namespace cobalt {
namespace audio {

#if defined(COBALT_MEDIA_SOURCE_2016)
typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

// numberOfInputs  : 0
// numberOfOutputs : 1
AudioBufferSourceNode::AudioBufferSourceNode(AudioContext* context)
    : AudioNode(context), state_(kNone), read_index_(0) {
  AudioLock::AutoLock lock(audio_lock());

  AddOutput(new AudioNodeOutput(this));
}

AudioBufferSourceNode::~AudioBufferSourceNode() {
  AudioLock::AutoLock lock(audio_lock());

  DCHECK_EQ(number_of_inputs(), 0u);
  RemoveAllOutputs();
}

void AudioBufferSourceNode::set_buffer(
    const scoped_refptr<AudioBuffer>& buffer) {
  AudioLock::AutoLock lock(audio_lock());

  buffer_ = buffer;
}

void AudioBufferSourceNode::Start(double when, double offset,
                                  script::ExceptionState* exception_state) {
  Start(when, offset, 0, exception_state);
}

// TODO: Fully implement start and stop method. The starting time is
// based on the current time of AudioContext. We only support start at 0 and
// stop at 0 currently.
void AudioBufferSourceNode::Start(double when, double offset, double duration,
                                  script::ExceptionState* exception_state) {
  AudioLock::AutoLock lock(audio_lock());

  if (when != 0 || offset != 0 || duration != 0) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (state_ != kNone) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  state_ = kStarted;
}

void AudioBufferSourceNode::Stop(double when,
                                 script::ExceptionState* exception_state) {
  AudioLock::AutoLock lock(audio_lock());

  if (when != 0) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (state_ != kStarted) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  state_ = kStopped;
}

scoped_ptr<ShellAudioBus> AudioBufferSourceNode::PassAudioBusFromSource(
    int32 number_of_frames, SampleType sample_type, bool* finished) {
  DCHECK_GT(number_of_frames, 0);
  DCHECK(finished);

  // This is called by Audio thread.
  audio_lock()->AssertLocked();

  *finished = false;

  if (state_ == kNone || !buffer_) {
    return scoped_ptr<ShellAudioBus>();
  }

  if (state_ == kStopped || buffer_->length() == read_index_) {
    *finished = true;
    return scoped_ptr<ShellAudioBus>();
  }

  DCHECK_EQ(state_, kStarted);

  auto audio_bus = buffer_->audio_bus();
  DCHECK_EQ(sample_type, audio_bus->sample_type());

  int32 frames_to_end = buffer_->length() - read_index_;
  number_of_frames = std::min(number_of_frames, frames_to_end);

  scoped_ptr<ShellAudioBus> result;

  if (sample_type == kSampleTypeInt16) {
    result.reset(new media::ShellAudioBus(
        audio_bus->channels(), number_of_frames,
        reinterpret_cast<int16*>(audio_bus->interleaved_data()) +
            read_index_ * audio_bus->channels()));
  } else {
    DCHECK_EQ(sample_type, kSampleTypeFloat32);

    result.reset(new media::ShellAudioBus(
        audio_bus->channels(), number_of_frames,
        reinterpret_cast<float*>(audio_bus->interleaved_data()) +
            read_index_ * audio_bus->channels()));
  }

  read_index_ += number_of_frames;
  return result.Pass();
}

void AudioBufferSourceNode::TraceMembers(script::Tracer* tracer) {
  AudioNode::TraceMembers(tracer);

  tracer->Trace(buffer_);
}

}  // namespace audio
}  // namespace cobalt

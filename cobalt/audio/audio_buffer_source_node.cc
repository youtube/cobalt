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
  state_ = kStoped;
}

scoped_ptr<ShellAudioBus> AudioBufferSourceNode::PassAudioBusFromSource(
    int32 number_of_frames, SampleType sample_type) {
  // This is called by Audio thread.
  audio_lock()->AssertLocked();

  if (state_ != kStarted || !buffer_ || buffer_->length() == read_index_) {
    return scoped_ptr<ShellAudioBus>();
  }

  DCHECK_GT(number_of_frames, 0);
  int32 frames_to_end = buffer_->length() - read_index_;
  number_of_frames = std::min(number_of_frames, frames_to_end);

  size_t channels = static_cast<size_t>(buffer_->number_of_channels());

  if (sample_type == kSampleTypeFloat32) {
    std::vector<scoped_refptr<dom::Float32Array>> audio_buffer_storages(
        channels);
    std::vector<float*> audio_buffers(channels, NULL);
    for (size_t i = 0; i < channels; ++i) {
      scoped_refptr<dom::Float32Array> buffer_data =
          buffer_->GetChannelData(static_cast<uint32>(i), NULL);
      audio_buffer_storages[i] = buffer_data->Subarray(
          NULL, read_index_, read_index_ + number_of_frames);
      audio_buffers[i] = audio_buffer_storages[i]->data();
    }

    read_index_ += number_of_frames;

    scoped_ptr<ShellAudioBus> audio_bus(new ShellAudioBus(
        static_cast<size_t>(number_of_frames), audio_buffers));

    return audio_bus.Pass();
  } else if (sample_type == kSampleTypeInt16) {
    std::vector<scoped_refptr<dom::Int16Array>> audio_buffer_storages(channels);
    std::vector<int16*> audio_buffers(channels, NULL);
    for (size_t i = 0; i < channels; ++i) {
      scoped_refptr<dom::Int16Array> buffer_data =
          buffer_->GetChannelDataInt16(static_cast<uint32>(i), NULL);
      audio_buffer_storages[i] = buffer_data->Subarray(
          NULL, read_index_, read_index_ + number_of_frames);
      audio_buffers[i] = audio_buffer_storages[i]->data();
    }

    read_index_ += number_of_frames;

    scoped_ptr<ShellAudioBus> audio_bus(new ShellAudioBus(
        static_cast<size_t>(number_of_frames), audio_buffers));

    return audio_bus.Pass();
  }

  NOTREACHED();
  return scoped_ptr<ShellAudioBus>();
}

void AudioBufferSourceNode::TraceMembers(script::Tracer* tracer) {
  AudioNode::TraceMembers(tracer);

  tracer->Trace(buffer_);
}

}  // namespace audio
}  // namespace cobalt

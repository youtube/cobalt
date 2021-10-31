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

#include "cobalt/audio/audio_buffer_source_node.h"

#include <math.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "cobalt/audio/audio_context.h"
#include "cobalt/audio/audio_helpers.h"
#include "cobalt/audio/audio_node_output.h"

namespace cobalt {
namespace audio {

typedef media::InterleavedSincResampler InterleavedSincResampler;
typedef media::AudioBus AudioBus;

// numberOfInputs  : 0
// numberOfOutputs : 1
AudioBufferSourceNode::AudioBufferSourceNode(
    script::EnvironmentSettings* settings, AudioContext* context)
    : AudioNode(settings, context),
      task_runner_(base::MessageLoop::current()->task_runner()),
      state_(kNone),
      read_index_(0),
      buffer_source_added_(false),
      sample_rate_(context->sample_rate()) {
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

  if (buffer_->sample_rate() != sample_rate_) {
    interleaved_resampler_ =
        std::unique_ptr<InterleavedSincResampler>(new InterleavedSincResampler(
            buffer_->sample_rate() / sample_rate_,
            static_cast<int32>(buffer_->audio_bus()->channels())));
  }

  // TODO: Find a more optimal way of holding a reference to the
  // AudioBufferSourceNode. This is not ideal because AudioNodes are organized
  // as a tree; this puts the ownership of a leaf node to the root of the tree.
  context()->AddBufferSource(base::WrapRefCounted(this));
  buffer_source_added_ = true;
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

std::unique_ptr<AudioBus> AudioBufferSourceNode::PassAudioBusFromSource(
    int32 number_of_frames, SampleType sample_type, bool* finished) {
  DCHECK_GT(number_of_frames, 0);
  DCHECK(finished);

  // This is called by Audio thread.
  audio_lock()->AssertLocked();

  *finished = false;

  if (state_ == kNone || !buffer_) {
    return std::unique_ptr<AudioBus>();
  }

  if (state_ == kStopped ||
      (!interleaved_resampler_ && read_index_ == buffer_->length()) ||
      (interleaved_resampler_ && interleaved_resampler_->ReachedEOS())) {
    *finished = true;

    if (buffer_source_added_) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&AudioBufferSourceNode::RemoveBufferSource,
                                base::Unretained(this)));
      buffer_source_added_ = false;
    }
    return std::unique_ptr<AudioBus>();
  }

  DCHECK_EQ(state_, kStarted);

  auto audio_bus = buffer_->audio_bus();
  DCHECK_EQ(sample_type, audio_bus->sample_type());

  int32 frames_to_end = buffer_->length() - read_index_;
  int32 channel_count = static_cast<int32>(audio_bus->channels());

  std::unique_ptr<AudioBus> result;

  if (!interleaved_resampler_) {
    int32 audio_bus_frames = std::min(number_of_frames, frames_to_end);
    if (sample_type == kSampleTypeInt16) {
      result.reset(
          new AudioBus(channel_count, audio_bus_frames,
                       reinterpret_cast<int16*>(audio_bus->interleaved_data()) +
                           read_index_ * channel_count));
    } else {
      DCHECK_EQ(sample_type, kSampleTypeFloat32);

      result.reset(
          new AudioBus(channel_count, audio_bus_frames,
                       reinterpret_cast<float*>(audio_bus->interleaved_data()) +
                           read_index_ * channel_count));
    }
    read_index_ += audio_bus_frames;
    return result;
  }

  // Resample audio if the audio buffer sample rate is not equal to the audio
  // context sample rate.

  // Queue frames.
  while (!interleaved_resampler_->HasEnoughData(number_of_frames)) {
    int32 frames_to_queue = static_cast<int32>(
        ceil(number_of_frames * buffer_->sample_rate() / sample_rate_));

    frames_to_queue = std::min(frames_to_queue, frames_to_end);

    if (sample_type == kSampleTypeInt16) {
      int16* samples_in_int16 =
          reinterpret_cast<int16*>(audio_bus->interleaved_data()) +
          read_index_ * channel_count;
      std::unique_ptr<float[]> samples_in_float(
          new float[frames_to_queue * channel_count]);
      for (int32 i = 0; i < frames_to_queue * channel_count; ++i) {
        samples_in_float[i] = ConvertSample<int16, float>(samples_in_int16[i]);
      }

      interleaved_resampler_->QueueBuffer(std::move(samples_in_float),
                                          frames_to_queue);
    } else {
      DCHECK_EQ(sample_type, kSampleTypeFloat32);

      float* samples_in_float =
          reinterpret_cast<float*>(audio_bus->interleaved_data()) +
          read_index_ * channel_count;
      interleaved_resampler_->QueueBuffer(samples_in_float, frames_to_queue);
    }

    read_index_ += frames_to_queue;
    frames_to_end = buffer_->length() - read_index_;

    // Last time queueing buffer: signify end of stream.
    if (read_index_ == buffer_->length()) {
      interleaved_resampler_->QueueBuffer(NULL, 0);
    }
  }

  // Write resampled frames.
  if (sample_type == kSampleTypeInt16) {
    std::unique_ptr<float[]> interleaved_output(
        new float[number_of_frames * channel_count]);
    interleaved_resampler_->Resample(interleaved_output.get(),
                                     number_of_frames);

    result.reset(new AudioBus(channel_count, number_of_frames, kSampleTypeInt16,
                              kStorageTypeInterleaved));
    for (int32 i = 0; i < channel_count * number_of_frames; ++i) {
      uint8* dest_ptr = result->interleaved_data() + sizeof(int16) * i;
      *reinterpret_cast<int16*>(dest_ptr) =
          ConvertSample<float, int16>(interleaved_output[i]);
    }
  } else {
    DCHECK_EQ(sample_type, kSampleTypeFloat32);

    result.reset(new AudioBus(channel_count, number_of_frames,
                              kSampleTypeFloat32, kStorageTypeInterleaved));
    interleaved_resampler_->Resample(
        reinterpret_cast<float*>(result->interleaved_data()), number_of_frames);
  }

  return result;
}

void AudioBufferSourceNode::TraceMembers(script::Tracer* tracer) {
  AudioNode::TraceMembers(tracer);

  tracer->Trace(buffer_.get());
}

void AudioBufferSourceNode::RemoveBufferSource() {
  context()->RemoveBufferSource(base::WrapRefCounted(this));
  DispatchEvent(new dom::Event(base::Tokens::ended()));
}

}  // namespace audio
}  // namespace cobalt

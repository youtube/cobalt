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

#include <algorithm>
#include <limits>
#include <memory>

#include "cobalt/audio/audio_buffer.h"

#include "cobalt/audio/audio_helpers.h"
#include "cobalt/dom/dom_exception.h"
#include "starboard/memory.h"

namespace cobalt {
namespace audio {

AudioBuffer::AudioBuffer(float sample_rate, std::unique_ptr<AudioBus> audio_bus)
    : sample_rate_(sample_rate), audio_bus_(std::move(audio_bus)) {
  DCHECK_GT(sample_rate_, 0);
  DCHECK_GT(length(), 0);
  DCHECK_GT(number_of_channels(), 0);
}

void AudioBuffer::CopyToChannel(
    const script::Handle<script::Float32Array>& source, uint32 channel_number,
    uint32 start_in_channel, script::ExceptionState* exception_state) {
  if (channel_number >= audio_bus_->channels() ||
      start_in_channel > audio_bus_->frames()) {
    dom::DOMException::Raise(dom::DOMException::kIndexSizeErr, exception_state);
    return;
  }

  size_t last_frame =
      std::min(audio_bus_->frames(), source->Length() + start_in_channel);
  float* src_data = source->Data();
  switch (audio_bus_->sample_type()) {
    case kSampleTypeFloat32: {
      if (audio_bus_->storage_type() == kStorageTypePlanar) {
        for (size_t frame = start_in_channel; frame < last_frame; ++frame) {
          float* dest_ptr = reinterpret_cast<float*>(
              audio_bus_->GetSamplePtrForType<float, kStorageTypePlanar>(
                  channel_number, frame));
          *dest_ptr = *src_data;
          ++src_data;
        }
      } else {
        for (size_t frame = start_in_channel; frame < last_frame; ++frame) {
          float* dest_ptr = reinterpret_cast<float*>(
              audio_bus_->GetSamplePtrForType<float, kStorageTypeInterleaved>(
                  channel_number, frame));
          *dest_ptr = *src_data;
          ++src_data;
        }
      }
      break;
    }
    case kSampleTypeInt16: {
      if (audio_bus_->storage_type() == kStorageTypePlanar) {
        for (size_t frame = start_in_channel; frame < last_frame; ++frame) {
          uint8* dest_ptr =
              audio_bus_->GetSamplePtrForType<int16, kStorageTypePlanar>(
                  channel_number, frame);
          *reinterpret_cast<int16*>(dest_ptr) =
              ConvertSample<float, int16>(*src_data);
          ++src_data;
        }
      } else {
        for (size_t frame = start_in_channel; frame < last_frame; ++frame) {
          uint8* dest_ptr =
              audio_bus_->GetSamplePtrForType<int16, kStorageTypeInterleaved>(
                  channel_number, frame);
          *reinterpret_cast<int16*>(dest_ptr) =
              ConvertSample<float, int16>(*src_data);
          ++src_data;
        }
      }
      break;
    }
  }
}

}  // namespace audio
}  // namespace cobalt

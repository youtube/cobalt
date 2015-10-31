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

#include "cobalt/audio/audio_buffer.h"

#include "cobalt/dom/dom_exception.h"

namespace cobalt {
namespace audio {

AudioBuffer::AudioBuffer(float sample_rate, int32 number_of_frames,
                         const Float32ArrayVector& channels_data)
    : sample_rate_(sample_rate),
      length_(number_of_frames),
      channels_data_(channels_data) {
  DCHECK_GT(sample_rate_, 0);
  DCHECK_GT(length_, 0);
  DCHECK(!channels_data_.empty());
}

scoped_refptr<dom::Float32Array> AudioBuffer::GetChannelData(
    uint32 channel_index, script::ExceptionState* exception_state) const {
  // The index value MUST be less than number_of_channels() or an INDEX_SIZE_ERR
  // exception MUST be thrown.
  if (channel_index >= static_cast<uint32>(number_of_channels())) {
    dom::DOMException::Raise(dom::DOMException::kIndexSizeErr, exception_state);
    return NULL;
  }

  return channels_data_[channel_index];
}

}  // namespace audio
}  // namespace cobalt

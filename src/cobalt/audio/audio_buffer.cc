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

AudioBuffer::AudioBuffer(script::EnvironmentSettings* settings,
                         float sample_rate, int32 number_of_frames,
                         int32 number_of_channels,
                         scoped_array<uint8> channels_data)
    : sample_rate_(sample_rate), length_(number_of_frames) {
  DCHECK_GT(sample_rate_, 0);
  DCHECK_GT(length_, 0);
  DCHECK_GT(number_of_channels, 0);

  // Create an ArrayBuffer stores sample data from all channels.
  scoped_refptr<dom::ArrayBuffer> array_buffer(new dom::ArrayBuffer(
      settings, dom::ArrayBuffer::kFromHeap, channels_data.Pass(),
      number_of_frames * number_of_channels * sizeof(float)));

  channels_data_.resize(static_cast<size_t>(number_of_channels));
  // Each channel should have |number_of_frames * sizeof(float)| bytes.  We
  // create |number_of_channels| of Float32Array as views into the above
  // ArrayBuffer, this doesn't need any extra allocation.
  uint32 start_offset_in_bytes = 0;
  for (int32 i = 0; i < number_of_channels; ++i) {
    channels_data_[static_cast<size_t>(i)] =
        new dom::Float32Array(settings, array_buffer, start_offset_in_bytes,
                              static_cast<uint32>(number_of_frames), NULL);
    start_offset_in_bytes += number_of_frames * sizeof(float);
  }
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

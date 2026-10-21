// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_libc_calls
#endif

#include "cobalt/media/audio/audio_decoder.h"

#include <stdint.h>

#include <utility>

#include "base/logging.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/public/platform/web_audio_bus.h"

namespace cobalt {

// Only supports PCM16 wav files.
bool DecodeAudioFileData(blink::WebAudioBus* destination_bus,
                         base::span<const char> audio_file_data) {
  DCHECK(destination_bus);
  if (!destination_bus) {
    return false;
  }

  LOG(INFO) << "Cobalt WAV decoder initializing..";
  auto handler =
      media::WavAudioHandler::Create(base::as_bytes(audio_file_data));

  if (!handler) {
    LOG(ERROR) << "Failed to create WavAudioHandler.";
    return false;
  }

  std::unique_ptr<media::AudioBus> source_bus = media::AudioBus::Create(
      handler->GetNumChannels(), handler->total_frames_for_testing());
  size_t number_of_frames = 0u;
  handler->CopyTo(source_bus.get(), &number_of_frames);
  DCHECK_EQ(number_of_frames,
            static_cast<size_t>(handler->total_frames_for_testing()));
  if (number_of_frames <= 0) {
    return false;
  }

  // Allocate and configure the output audio channel data and then
  // copy the decoded data to the destination.
  destination_bus->Initialize(handler->GetNumChannels(), number_of_frames,
                              handler->GetSampleRate());

  DCHECK_EQ(static_cast<int>(destination_bus->NumberOfChannels()),
            handler->GetNumChannels());
  DCHECK_EQ(destination_bus->length(), number_of_frames);

  if (std::cmp_equal(source_bus->channels(),
                     destination_bus->NumberOfChannels()) &&
      std::cmp_equal(source_bus->frames(), destination_bus->length())) {
    size_t bytes_per_channel = source_bus->frames() * sizeof(float);
    for (int channel_index = 0; channel_index < source_bus->channels();
         ++channel_index) {
      const float* source_data = source_bus->channel(channel_index);
      float* dest_data = destination_bus->ChannelData(channel_index);
      memcpy(dest_data, source_data, bytes_per_channel);
    }

  } else {
    LOG(ERROR) << "AudioBus dimension mismatch during copy.\nSource Channels:"
               << source_bus->channels()
               << " Dest Channels:" << destination_bus->NumberOfChannels()
               << " Source Frames:" << source_bus->frames()
               << " Dest Frames:" << destination_bus->length();
    return false;
  }

  return number_of_frames > 0;
}

}  // namespace cobalt

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

#include "cobalt/media/audio/audio_decoder.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "third_party/blink/public/platform/web_audio_bus.h"

namespace cobalt {

// Only supports PCM16 wav files.
bool DecodeAudioFileData(blink::WebAudioBus* destination_bus,
                         const char* data,
                         size_t data_size) {
  DCHECK(destination_bus);
  if (!destination_bus) {
    return false;
  }

  LOG(INFO) << "Cobalt WAV decoder initializing..";
  auto handler =
      media::WavAudioHandler::Create(base::StringPiece(data, data_size));

  if (!handler) {
    LOG(ERROR) << "Failed to create WavAudioHandler.";
    return false;
  }

  std::unique_ptr<media::AudioBus> bus = media::AudioBus::Create(
      handler->GetNumChannels(),
      handler->data().size() / handler->GetNumChannels());
  size_t number_of_frames = 0u;
  handler->CopyTo(bus.get(), &number_of_frames);

  if (number_of_frames <= 0) {
    return false;
  }

  // Allocate and configure the output audio channel data and then
  // copy the decoded data to the destination.
  destination_bus->Initialize(handler->GetNumChannels(), number_of_frames,
                              handler->GetSampleRate());
  bus->ToInterleaved<media::Float32SampleTypeTraits>(
      number_of_frames, destination_bus->ChannelData(0));

  return number_of_frames > 0;
}

}  // namespace cobalt

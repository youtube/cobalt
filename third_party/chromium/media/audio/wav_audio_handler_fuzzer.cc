// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "third_party/chromium/media/audio/wav_audio_handler.h"
#include "third_party/chromium/media/base/audio_bus.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  base::StringPiece wav_data(reinterpret_cast<const char*>(data), size);
  std::unique_ptr<media_m96::WavAudioHandler> handler =
      media_m96::WavAudioHandler::Create(wav_data);

  // Abort early to avoid crashing inside AudioBus's ValidateConfig() function.
  if (!handler || handler->total_frames() <= 0) {
    return 0;
  }

  std::unique_ptr<media_m96::AudioBus> audio_bus =
      media_m96::AudioBus::Create(handler->num_channels(), handler->total_frames());
  size_t bytes_written;
  handler->CopyTo(audio_bus.get(), 0, &bytes_written);
  return 0;
}

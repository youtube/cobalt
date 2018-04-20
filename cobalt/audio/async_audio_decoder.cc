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

#include "cobalt/audio/async_audio_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/audio/audio_file_reader.h"
#include "cobalt/audio/audio_helpers.h"

namespace cobalt {
namespace audio {

namespace {

// Perform the decoding operation. The decoding thread will attempt to decode
// the encoded audio_data into linear PCM.
void Decode(
    const uint8* audio_data, size_t size,
    const AsyncAudioDecoder::DecodeFinishCallback& decode_finish_callback) {
  scoped_ptr<AudioFileReader> reader(AudioFileReader::TryCreate(
      audio_data, size, GetPreferredOutputSampleType()));

  if (reader) {
    decode_finish_callback.Run(reader->sample_rate(),
                               reader->ResetAndReturnAudioBus().Pass());
  } else {
    decode_finish_callback.Run(0.f, scoped_ptr<ShellAudioBus>());
  }
}

}  // namespace

AsyncAudioDecoder::AsyncAudioDecoder() : thread_("AsyncAudioDecoder") {
  thread_.Start();
}

void AsyncAudioDecoder::Stop() { thread_.Stop(); }

void AsyncAudioDecoder::AsyncDecode(
    const uint8* audio_data, size_t size,
    const DecodeFinishCallback& decode_finish_callback) {
  DCHECK(audio_data);
  DCHECK(!decode_finish_callback.is_null());

  // Queue a decoding operation to be performed on AsyncAudioDecoder thread.
  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&Decode, audio_data, size, decode_finish_callback));
}

}  // namespace audio
}  // namespace cobalt

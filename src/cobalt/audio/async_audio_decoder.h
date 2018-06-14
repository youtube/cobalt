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

#ifndef COBALT_AUDIO_ASYNC_AUDIO_DECODER_H_
#define COBALT_AUDIO_ASYNC_AUDIO_DECODER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "cobalt/audio/audio_helpers.h"

namespace cobalt {
namespace audio {

class AsyncAudioDecoder {
 public:
  typedef base::Callback<void(float sample_rate,
                              scoped_ptr<ShellAudioBus> audio_bus)>
      DecodeFinishCallback;

  AsyncAudioDecoder();

  void AsyncDecode(const uint8* audio_data, size_t size,
                   const DecodeFinishCallback& decode_finish_callback);
  void Stop();

 private:
  // The decoder thread.
  base::Thread thread_;

  DISALLOW_COPY_AND_ASSIGN(AsyncAudioDecoder);
};

}  // namespace audio
}  // namespace cobalt

#endif  // COBALT_AUDIO_ASYNC_AUDIO_DECODER_H_

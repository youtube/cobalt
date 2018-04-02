// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_AUDIO_DECODER_THREAD_H_
#define STARBOARD_SHARED_WIN32_AUDIO_DECODER_THREAD_H_

#include <deque>
#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/thread.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/win32_audio_decoder.h"

namespace starboard {
namespace shared {
namespace win32 {

class AudioDecodedCallback {
 public:
  virtual ~AudioDecodedCallback() {}
  virtual void OnAudioDecoded(DecodedAudioPtr data) = 0;
};

// This decoder thread simplifies decoding media. Data is pushed in via
// QueueInput() and QueueEndOfStream() and output data is pushed via
// the AudioDecodedCallback.
class AudioDecoderThread : private Thread {
 public:
  AudioDecoderThread(AbstractWin32AudioDecoder* decoder_impl,
                     AudioDecodedCallback* callback);
  ~AudioDecoderThread() override;

  // Returns true if more input can be pushed to this thread.
  bool QueueInput(const scoped_refptr<InputBuffer>& buffer);
  void QueueEndOfStream();

 private:
  void Run() override;
  void TransferPendingInputTo(
      std::deque<scoped_refptr<InputBuffer> >* destination);
  AbstractWin32AudioDecoder* win32_audio_decoder_;
  AudioDecodedCallback* callback_;

  std::deque<scoped_refptr<InputBuffer> > input_buffer_queue_;
  ::starboard::Mutex input_buffer_queue_mutex_;
  atomic_int32_t processing_elements_;
  Semaphore semaphore_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_AUDIO_DECODER_THREAD_H_

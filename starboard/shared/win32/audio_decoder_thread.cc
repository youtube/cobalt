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

#include "starboard/shared/win32/audio_decoder_thread.h"

#include <deque>
#include <vector>

namespace starboard {
namespace shared {
namespace win32 {
namespace {

// Size of the queue for audio units.
const size_t kMaxProcessingElements = 64;

size_t WriteAsMuchAsPossible(
    std::deque<scoped_refptr<InputBuffer> >* data_queue,
    AbstractWin32AudioDecoder* audio_decoder) {
  const size_t original_size = data_queue->size();
  while (!data_queue->empty()) {
    scoped_refptr<InputBuffer> buff = data_queue->front();
    data_queue->pop_front();

    if (buff) {
      if (!audio_decoder->TryWrite(buff)) {
        data_queue->push_front(buff);
        break;
      }
    } else {
      audio_decoder->WriteEndOfStream();
    }
  }
  return original_size - data_queue->size();
}

std::vector<DecodedAudioPtr> ReadAllDecodedAudioSamples(
    AbstractWin32AudioDecoder* audio_decoder) {
  std::vector<DecodedAudioPtr> decoded_audio_out;
  while (DecodedAudioPtr decoded_datum = audio_decoder->ProcessAndRead()) {
    decoded_audio_out.push_back(decoded_datum);
  }
  return decoded_audio_out;
}

}  // namespace.

AudioDecoderThread::AudioDecoderThread(AbstractWin32AudioDecoder* decoder_impl,
                                       AudioDecodedCallback* callback)
    : Thread("AudioDecoderThread"),
      win32_audio_decoder_(decoder_impl),
      callback_(callback) {
  Start();
}

AudioDecoderThread::~AudioDecoderThread() {
  Join();
}

bool AudioDecoderThread::QueueInput(const scoped_refptr<InputBuffer>& buffer) {
  {
    ::starboard::ScopedLock lock(input_buffer_queue_mutex_);
    input_buffer_queue_.push_back(buffer);
  }

  // increment() returns the previous value.
  size_t element_count = processing_elements_.increment() + 1;
  semaphore_.Put();
  return element_count < kMaxProcessingElements;
}

void AudioDecoderThread::QueueEndOfStream() {
  scoped_refptr<InputBuffer> empty;
  QueueInput(empty);
}

void AudioDecoderThread::Run() {
  std::deque<scoped_refptr<InputBuffer> > local_queue;

  while (!join_called()) {
    if (local_queue.empty()) {
      TransferPendingInputTo(&local_queue);
    }
    bool work_done = false;
    size_t number_written =
        WriteAsMuchAsPossible(&local_queue, win32_audio_decoder_);
    if (number_written > 0) {
      processing_elements_.fetch_sub(static_cast<int32_t>(number_written));
      work_done = true;
    }

    std::vector<DecodedAudioPtr> decoded_audio =
        ReadAllDecodedAudioSamples(win32_audio_decoder_);

    if (!decoded_audio.empty()) {
      work_done = true;
      for (auto it = decoded_audio.begin(); it != decoded_audio.end(); ++it) {
        callback_->OnAudioDecoded(*it);
      }
    }

    if (!work_done) {
      semaphore_.TakeWait(kSbTimeMillisecond);
    }
  }
}

void AudioDecoderThread::TransferPendingInputTo(
    std::deque<scoped_refptr<InputBuffer> >* destination) {
  ::starboard::ScopedLock lock(input_buffer_queue_mutex_);
  while (!input_buffer_queue_.empty()) {
    destination->push_back(input_buffer_queue_.front());
    input_buffer_queue_.pop_front();
  }
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

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

#include "starboard/shared/win32/video_decoder_thread.h"

#include <deque>

namespace starboard {
namespace shared {
namespace win32 {

namespace {
const size_t kMaxSize = 16;

size_t WriteAsMuchAsPossible(
    std::deque<scoped_refptr<InputBuffer> >* data_queue,
    AbstractWin32VideoDecoder* video_decoder,
    bool* is_end_of_stream_reached) {
  const size_t original_size = data_queue->size();
  while (!data_queue->empty()) {
    scoped_refptr<InputBuffer> buff = data_queue->front();
    data_queue->pop_front();

    if (buff) {
      const bool write_ok = video_decoder->TryWrite(buff);

      if (!write_ok) {
        data_queue->push_front(buff);
        break;
      }
    } else {
      video_decoder->WriteEndOfStream();
      *is_end_of_stream_reached = true;
    }
  }
  return original_size - data_queue->size();
}

}  // namespace.

VideoDecoderThread::VideoDecoderThread(AbstractWin32VideoDecoder* decoder_impl,
                                       VideoDecodedCallback* callback)
    : SimpleThread("VideoDecoderThread"),
      win32_video_decoder_(decoder_impl),
      callback_(callback) {
  Start();
}

VideoDecoderThread::~VideoDecoderThread() {
  Join();
  SB_DCHECK(join_called());
}

bool VideoDecoderThread::QueueInput(const scoped_refptr<InputBuffer>& buffer) {
  {
    ::starboard::ScopedLock lock(input_buffer_queue_mutex_);
    input_buffer_queue_.push_back(buffer);
  }

  // increment() returns the prev value.
  size_t proc_size = processing_elements_.increment() + 1;
  semaphore_.Put();
  return proc_size < kMaxSize;
}

void VideoDecoderThread::QueueEndOfStream() {
  scoped_refptr<InputBuffer> empty;
  QueueInput(empty);
}

void VideoDecoderThread::TransferPendingInputTo(
    std::deque<scoped_refptr<InputBuffer> >* output) {
  // Transfer input buffer to local thread.
  ::starboard::ScopedLock lock(input_buffer_queue_mutex_);
  while (!input_buffer_queue_.empty()) {
    output->push_back(input_buffer_queue_.front());
    input_buffer_queue_.pop_front();
  }
}

void VideoDecoderThread::Run() {
  std::deque<scoped_refptr<InputBuffer> > local_queue;
  while (!join_called()) {
    // Transfer input buffer to local thread.
    TransferPendingInputTo(&local_queue);
    bool work_done = !local_queue.empty();
    bool is_end_of_stream = false;
    const size_t number_written =
        WriteAsMuchAsPossible(&local_queue, win32_video_decoder_,
                              &is_end_of_stream);
    if (number_written > 0) {
      processing_elements_.fetch_sub(static_cast<int32_t>(number_written));
      work_done = true;
    }

    while (VideoFramePtr decoded_datum =
             win32_video_decoder_->ProcessAndRead()) {
      if (decoded_datum.get()) {
        callback_->OnVideoDecoded(decoded_datum);
      }
      work_done = true;
    }

    if (is_end_of_stream) {
      callback_->OnVideoDecoded(VideoFrame::CreateEOSFrame());
      work_done = true;
    }

    if (!work_done) {
      semaphore_.TakeWait(kSbTimeMillisecond);
    }
  }
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

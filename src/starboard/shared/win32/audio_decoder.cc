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

#include "starboard/shared/win32/audio_decoder.h"

#include "starboard/atomic.h"
#include "starboard/audio_sink.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/media_common.h"

namespace starboard {
namespace shared {
namespace win32 {

class AudioDecoder::CallbackScheduler : private JobOwner {
 public:
  CallbackScheduler() : callback_signaled_(false) {}

  void SetCallbackOnce(Closure cb) {
    SB_DCHECK(cb.is_valid());
    ::starboard::ScopedLock lock(mutex_);
    if (!cb_.is_valid()) {
      cb_ = cb;
    }
  }

  void ScheduleCallbackIfNecessary() {
    ::starboard::ScopedLock lock(mutex_);
    if (!cb_.is_valid() || callback_signaled_) {
      return;
    }
    callback_signaled_ = true;
    JobOwner::Schedule(cb_);
  }

  void OnCallbackSignaled() {
    ::starboard::ScopedLock lock(mutex_);
    callback_signaled_ = false;
  }

  Closure cb_;
  ::starboard::Mutex mutex_;
  bool callback_signaled_;
};

AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioHeader& audio_header)
    : sample_type_(kSbMediaAudioSampleTypeFloat32),
      stream_ended_(false),
      audio_codec_(audio_codec),
      audio_header_(audio_header) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAac);
  decoder_impl_ = AbstractWin32AudioDecoder::Create(
      audio_codec_, GetStorageType(), GetSampleType(), audio_header_);
  decoder_thread_.reset(new AudioDecoderThread(decoder_impl_.get(), this));
  callback_scheduler_.reset(new CallbackScheduler());
}

AudioDecoder::~AudioDecoder() {
  decoder_thread_.reset(nullptr);
  decoder_impl_.reset(nullptr);
  callback_scheduler_.reset(nullptr);
}

void AudioDecoder::Decode(const scoped_refptr<InputBuffer>& input_buffer,
                          const Closure& consumed_cb) {
  SB_DCHECK(input_buffer);
  callback_scheduler_->SetCallbackOnce(consumed_cb);
  callback_scheduler_->OnCallbackSignaled();
  const bool can_take_more_data = decoder_thread_->QueueInput(input_buffer);
  if (can_take_more_data) {
    callback_scheduler_->ScheduleCallbackIfNecessary();
  }

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }
}

void AudioDecoder::WriteEndOfStream() {
  ::starboard::ScopedLock lock(mutex_);
  stream_ended_ = true;
  decoder_thread_->QueueEndOfStream();
}

scoped_refptr<AudioDecoder::DecodedAudio> AudioDecoder::Read() {
  DecodedAudioPtr data = decoded_data_.PopFront();
  SB_DCHECK(data);
  return data;
}

void AudioDecoder::Reset() {
  decoder_thread_.reset(nullptr);
  decoder_impl_.reset(nullptr);
  decoder_impl_ = AbstractWin32AudioDecoder::Create(
      audio_codec_, GetStorageType(), GetSampleType(), audio_header_);
  decoder_thread_.reset(new AudioDecoderThread(decoder_impl_.get(), this));
  decoded_data_.Clear();
  stream_ended_ = false;
  CancelPendingJobs();
}

SbMediaAudioSampleType AudioDecoder::GetSampleType() const {
  return sample_type_;
}

int AudioDecoder::GetSamplesPerSecond() const {
  return audio_header_.samples_per_second;
}

void AudioDecoder::Initialize(const Closure& output_cb) {
  SB_DCHECK(output_cb.is_valid());
  SB_DCHECK(!output_cb_.is_valid());
  output_cb_ = output_cb;
}

void AudioDecoder::OnAudioDecoded(DecodedAudioPtr data) {
  decoded_data_.PushBack(data);
  if (output_cb_.is_valid()) {
    Schedule(output_cb_);
  }
  callback_scheduler_->ScheduleCallbackIfNecessary();
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

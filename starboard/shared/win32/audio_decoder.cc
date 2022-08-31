// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/media_common.h"

namespace starboard {
namespace shared {
namespace win32 {

class AudioDecoder::CallbackScheduler : private JobOwner {
 public:
  CallbackScheduler() : callback_signaled_(false) {}

  void SetCallbackOnce(ConsumedCB cb) {
    SB_DCHECK(cb);
    ::starboard::ScopedLock lock(mutex_);
    if (!cb_) {
      cb_ = cb;
    }
  }

  void ScheduleCallbackIfNecessary() {
    ::starboard::ScopedLock lock(mutex_);
    if (!cb_ || callback_signaled_) {
      return;
    }
    callback_signaled_ = true;
    JobOwner::Schedule(cb_);
  }

  void OnCallbackSignaled() {
    ::starboard::ScopedLock lock(mutex_);
    callback_signaled_ = false;
  }

  ConsumedCB cb_;
  ::starboard::Mutex mutex_;
  bool callback_signaled_;
};

AudioDecoder::AudioDecoder(SbMediaAudioCodec audio_codec,
                           const SbMediaAudioSampleInfo& audio_sample_info,
                           SbDrmSystem drm_system)
    : audio_codec_(audio_codec),
      audio_sample_info_(audio_sample_info),
      drm_system_(drm_system),
      sample_type_((audio_codec == kSbMediaAudioCodecAc3 ||
                    audio_codec == kSbMediaAudioCodecEac3)
                       ?
#if SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
                       kSbMediaAudioSampleTypeInt16
#else
                       kSbMediaAudioSampleTypeInt16Deprecated
#endif  // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
                       : kSbMediaAudioSampleTypeFloat32),
      stream_ended_(false) {
  SB_DCHECK(audio_codec == kSbMediaAudioCodecAac ||
            audio_codec == kSbMediaAudioCodecAc3 ||
            audio_codec == kSbMediaAudioCodecEac3);
}

AudioDecoder::~AudioDecoder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  decoder_thread_.reset(nullptr);
  decoder_impl_.reset(nullptr);
  callback_scheduler_.reset(nullptr);
}

void AudioDecoder::Initialize(const OutputCB& output_cb,
                              const ErrorCB& error_cb) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  output_cb_ = output_cb;
  decoder_impl_ = AbstractWin32AudioDecoder::Create(
      audio_codec_, kSbMediaAudioFrameStorageTypeInterleaved, sample_type_,
      audio_sample_info_, drm_system_);
  decoder_thread_.reset(new AudioDecoderThread(decoder_impl_.get(), this));
  callback_scheduler_.reset(new CallbackScheduler());
}

void AudioDecoder::Decode(const InputBuffers& input_buffers,
                          const ConsumedCB& consumed_cb) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);

  callback_scheduler_->SetCallbackOnce(consumed_cb);
  callback_scheduler_->OnCallbackSignaled();
  const bool can_take_more_data = decoder_thread_->QueueInput(input_buffers[0]);
  if (can_take_more_data) {
    callback_scheduler_->ScheduleCallbackIfNecessary();
  }

  if (stream_ended_) {
    SB_LOG(ERROR) << "Decode() is called after WriteEndOfStream() is called.";
    return;
  }
}

void AudioDecoder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  ::starboard::ScopedLock lock(mutex_);
  stream_ended_ = true;
  decoder_thread_->QueueEndOfStream();
}

scoped_refptr<AudioDecoder::DecodedAudio> AudioDecoder::Read(
    int* samples_per_second) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  DecodedAudioPtr data = decoded_data_.PopFront();
  SB_DCHECK(data);
  *samples_per_second = decoder_impl_->GetSamplesPerSecond();
  return data;
}

void AudioDecoder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  decoder_thread_.reset(nullptr);
  decoder_impl_->Reset();

  decoded_data_.Clear();
  stream_ended_ = false;
  callback_scheduler_.reset(new CallbackScheduler());
  CancelPendingJobs();

  decoder_thread_.reset(new AudioDecoderThread(decoder_impl_.get(), this));
}

void AudioDecoder::OnAudioDecoded(DecodedAudioPtr data) {
  decoded_data_.PushBack(data);
  if (output_cb_) {
    Schedule(output_cb_);
  }
  callback_scheduler_->ScheduleCallbackIfNecessary();
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

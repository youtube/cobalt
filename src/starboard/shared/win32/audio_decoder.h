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

#ifndef STARBOARD_SHARED_WIN32_AUDIO_DECODER_H_
#define STARBOARD_SHARED_WIN32_AUDIO_DECODER_H_

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/audio_decoder_thread.h"
#include "starboard/shared/win32/media_common.h"

namespace starboard {
namespace shared {
namespace win32 {

class AudioDecoder
    : public ::starboard::shared::starboard::player::filter::AudioDecoder,
      private ::starboard::shared::starboard::player::JobQueue::JobOwner,
      private AudioDecodedCallback {
 public:
  AudioDecoder(SbMediaAudioCodec audio_codec,
               const SbMediaAudioHeader& audio_header,
               SbDrmSystem drm_system);
  ~AudioDecoder() override;

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const scoped_refptr<InputBuffer>& input_buffer,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read() override;
  void Reset() override;
  SbMediaAudioSampleType GetSampleType() const override;
  int GetSamplesPerSecond() const override;

  SbMediaAudioFrameStorageType GetStorageType() const override {
    return kSbMediaAudioFrameStorageTypeInterleaved;
  }
  void OnAudioDecoded(DecodedAudioPtr data) override;

 private:
  class CallbackScheduler;

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  const SbMediaAudioCodec audio_codec_;
  const SbMediaAudioHeader audio_header_;
  SbDrmSystem const drm_system_;
  const SbMediaAudioSampleType sample_type_;
  bool stream_ended_;

  AtomicQueue<DecodedAudioPtr> decoded_data_;
  scoped_ptr<AudioDecoder::CallbackScheduler> callback_scheduler_;
  scoped_ptr<AbstractWin32AudioDecoder> decoder_impl_;
  scoped_ptr<AudioDecoderThread> decoder_thread_;
  OutputCB output_cb_;

  Mutex mutex_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_AUDIO_DECODER_H_

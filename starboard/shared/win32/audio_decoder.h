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

#ifndef STARBOARD_SHARED_WIN32_AUDIO_DECODER_H_
#define STARBOARD_SHARED_WIN32_AUDIO_DECODER_H_

#include <memory>

#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
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
  typedef starboard::media::AudioStreamInfo AudioStreamInfo;

  AudioDecoder(const AudioStreamInfo& audio_stream_info,
               SbDrmSystem drm_system);
  ~AudioDecoder() override;

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;
  void OnAudioDecoded(DecodedAudioPtr data) override;

 private:
  class CallbackScheduler;

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  const AudioStreamInfo audio_stream_info_;
  SbDrmSystem const drm_system_;
  const SbMediaAudioSampleType sample_type_;
  bool stream_ended_;

  AtomicQueue<DecodedAudioPtr> decoded_data_;
  std::unique_ptr<AudioDecoder::CallbackScheduler> callback_scheduler_;
  std::unique_ptr<AbstractWin32AudioDecoder> decoder_impl_;
  std::unique_ptr<AudioDecoderThread> decoder_thread_;
  OutputCB output_cb_;

  Mutex mutex_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_AUDIO_DECODER_H_

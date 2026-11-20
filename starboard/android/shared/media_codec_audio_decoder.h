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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_AUDIO_DECODER_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_AUDIO_DECODER_H_

#include <jni.h>

#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec.h"
#include "starboard/android/shared/media_codec_decoder.h"
#include "starboard/common/pass_key.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/thread.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_frame_discarder.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {

class MediaCodecAudioDecoder : public AudioDecoder,
                               public MediaCodecDecoder::Host,
                               private JobQueue::JobOwner {
 public:
  // |enable_async_codec_creation|: see |kMediaAsyncAudioCodecCreation|. Only
  // has an effect when |drm_system| is invalid. The decoder is then returned
  // before its MediaCodec exists, and a failure to create the MediaCodec is
  // reported through the error callback passed to Initialize() instead of by
  // the return value.
  // |media_codec_factory|: creates the MediaCodec. Must outlive the decoder.
  // Null for the factory used in production.
  static NonNullResult<std::unique_ptr<MediaCodecAudioDecoder>> Create(
      JobQueue* job_queue,
      const AudioStreamInfo& audio_stream_info,
      SbDrmSystem drm_system,
      bool enable_flush_during_seek,
      bool enable_async_codec_creation = false,
      MediaCodec::Factory* media_codec_factory = nullptr);

  MediaCodecAudioDecoder(PassKey<MediaCodecAudioDecoder>,
                         JobQueue* job_queue,
                         const AudioStreamInfo& audio_stream_info,
                         SbDrmSystem drm_system,
                         bool enable_flush_during_seek,
                         bool enable_async_codec_creation,
                         MediaCodec::Factory* media_codec_factory,
                         std::string* error_message);

  ~MediaCodecAudioDecoder() override;

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  std::optional<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;

 private:
  // The maximum amount of work that can exist in the union of |decoded_audios_|
  // and |media_decoder_->GetNumberOfPendingTasks()|.
  static const int kMaxPendingWorkSize = 64;

  // A call of Decode() made while the codec is being prepared.
  struct ParkedDecode {
    InputBuffers input_buffers;
    ConsumedCB consumed_cb;
  };

  class PrepareThread;

  Result<void> InitializeCodec();
  void DecodeInternal(const InputBuffers& input_buffers,
                      const ConsumedCB& consumed_cb);
  // Runs on |prepare_thread_|.
  void PrepareCodec();
  // Runs on the thread of the job queue once PrepareCodec() has finished.
  void OnCodecPrepared();
  size_t GetNumberOfParkedInputBuffers() const;
  void ProcessOutputBuffer(MediaCodec* media_codec_bridge,
                           const DequeueOutputResult& output,
                           int number_of_pending_inputs) override;
  void OnEndOfStreamWritten(MediaCodec* media_codec_bridge) override {}
  void RefreshOutputFormat(MediaCodec* media_codec_bridge) override;
  bool Tick(MediaCodec* media_codec_bridge) override { return false; }
  void OnFlushing() override {}
  bool IsBufferDecodeOnly(
      const scoped_refptr<InputBuffer>& input_buffer) override {
    return false;
  }

  void ReportError(SbPlayerError error, const std::string& error_message);

  const AudioStreamInfo audio_stream_info_;
  const SbMediaAudioSampleType sample_type_;
  const bool enable_flush_during_seek_;

  jint output_sample_rate_;
  jint output_channel_count_;

  DrmSystem* drm_system_;

  OutputCB output_cb_;
  ErrorCB error_cb_;
  ConsumedCB consumed_cb_;

  std::mutex decoded_audios_mutex_;
  std::queue<DecodedAudio> decoded_audios_;

  AudioFrameDiscarder audio_frame_discarder_;
  std::unique_ptr<MediaCodecDecoder> media_decoder_;

  DefaultMediaCodecFactory default_media_codec_factory_;
  MediaCodec::Factory* const media_codec_factory_;

  // The members below are only used with |kMediaAsyncAudioCodecCreation|.
  //
  // True from the constructor until OnCodecPrepared() has run: the codec is
  // being created and configured on |prepare_thread_|, or is waiting to be
  // started on this thread. |media_decoder_| is null and input is parked
  // meanwhile. Only used on the thread of the job queue.
  bool preparing_codec_ = false;
  // What was written while |preparing_codec_|, to be passed on once the codec
  // is ready. Dropped by Reset().
  std::vector<ParkedDecode> parked_decodes_;
  bool parked_end_of_stream_ = false;
  // An error that occurred before Initialize() provided |error_cb_|.
  std::optional<std::string> error_before_initialize_;
  int64_t prepare_started_at_us_ = 0;

  std::mutex prepare_mutex_;
  // The result of PrepareCodec(), null on failure. Guarded by |prepare_mutex_|.
  std::unique_ptr<MediaCodec::PreparedAudioCodec> prepared_codec_;
  // Guarded by |prepare_mutex_|.
  int64_t prepare_finished_at_us_ = 0;

  // Joined at the start of the destructor, so PrepareCodec() never outlives
  // this object.
  std::unique_ptr<Thread> prepare_thread_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_AUDIO_DECODER_H_

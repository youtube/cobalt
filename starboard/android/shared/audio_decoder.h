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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_DECODER_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_DECODER_H_

#include <jni.h>

#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/android/shared/media_decoder.h"
#include "starboard/common/ref_counted.h"
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
  static NonNullResult<std::unique_ptr<MediaCodecAudioDecoder>> Create(
      JobQueue* job_queue,
      const AudioStreamInfo& audio_stream_info,
      SbDrmSystem drm_system,
      bool enable_flush_during_seek);

  ~MediaCodecAudioDecoder() override;

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;

 private:
  MediaCodecAudioDecoder(JobQueue* job_queue,
                         const AudioStreamInfo& audio_stream_info,
                         SbDrmSystem drm_system,
                         bool enable_flush_during_seek,
                         std::string* error_message);

  // The maximum amount of work that can exist in the union of |decoded_audios_|
  // and |media_decoder_->GetNumberOfPendingTasks()|.
  static const int kMaxPendingWorkSize = 64;

  Result<void> InitializeCodec();
  void ProcessOutputBuffer(MediaCodecBridge* media_codec_bridge,
                           const DequeueOutputResult& output) override;
  void OnEndOfStreamWritten(MediaCodecBridge* media_codec_bridge) override {}
  void RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) override;
  bool Tick(MediaCodecBridge* media_codec_bridge) override { return false; }
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
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;

  AudioFrameDiscarder audio_frame_discarder_;
  std::unique_ptr<MediaCodecDecoder> media_decoder_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_DECODER_H_

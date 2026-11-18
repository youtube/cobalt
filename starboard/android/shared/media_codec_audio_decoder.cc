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

#include "starboard/android/shared/media_codec_audio_decoder.h"

#include <utility>

#include "starboard/android/shared/media_codec.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "third_party/jni_zero/jni_zero.h"

// Can be locally set to |1| for verbose audio decoding.  Verbose audio
// decoding will log the following transitions that take place for each audio
// unit:
//   T1: Our client passes multiple packets (i.e. InputBuffers) of audio data
//   into us.
//   T2: We receive a corresponding media codec output buffer back from our
//       |MediaCodecBridge|.
//   T3: Our client reads a corresponding |DecodedAudio| out of us.
//
// Example usage for debugging audio playback:
//   $ adb logcat -c
//   $ adb logcat | tee log.txt
//   # Play video and get to frozen point.
//   $ CTRL-C
//   $ cat log.txt | grep -P 'T2: pts \d+' | wc -l
//   523
//   $ cat log.txt | grep -P 'T3: pts \d+' | wc -l
//   522
//   # Oh no, why isn't our client reading the audio we have ready to go?
//   # Time to go find out...
#define STARBOARD_ANDROID_SHARED_AUDIO_DECODER_VERBOSE 0
#if STARBOARD_ANDROID_SHARED_AUDIO_DECODER_VERBOSE
#define VERBOSE_MEDIA_LOG() SB_LOG(INFO)
#else
#define VERBOSE_MEDIA_LOG() SB_EAT_STREAM_PARAMETERS
#endif

namespace starboard {
namespace {

using jni_zero::ScopedJavaLocalRef;
using std::placeholders::_1;
using std::placeholders::_2;

SbMediaAudioSampleType GetSupportedSampleType() {
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(
      kSbMediaAudioSampleTypeInt16Deprecated));
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

void* IncrementPointerByBytes(void* pointer, int offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

}  // namespace

class MediaCodecAudioDecoder::PrepareThread : public Thread {
 public:
  explicit PrepareThread(MediaCodecAudioDecoder* decoder)
      : Thread("aud_codec_prep"), decoder_(decoder) {}

  void Run() override { decoder_->PrepareCodec(); }

 private:
  MediaCodecAudioDecoder* const decoder_;
};

// static
NonNullResult<std::unique_ptr<MediaCodecAudioDecoder>>
MediaCodecAudioDecoder::Create(JobQueue* job_queue,
                               const AudioStreamInfo& audio_stream_info,
                               SbDrmSystem drm_system,
                               bool enable_flush_during_seek,
                               bool enable_async_codec_creation,
                               MediaCodec::Factory* media_codec_factory) {
  std::string error_message;
  auto audio_decoder = std::make_unique<MediaCodecAudioDecoder>(
      PassKey<MediaCodecAudioDecoder>(), job_queue, audio_stream_info,
      drm_system, enable_flush_during_seek, enable_async_codec_creation,
      media_codec_factory, &error_message);
  if (audio_decoder->media_decoder_ || audio_decoder->preparing_codec_) {
    return audio_decoder;
  }
  return Failure(error_message);
}

MediaCodecAudioDecoder::MediaCodecAudioDecoder(
    PassKey<MediaCodecAudioDecoder>,
    JobQueue* job_queue,
    const AudioStreamInfo& audio_stream_info,
    SbDrmSystem drm_system,
    bool enable_flush_during_seek,
    bool enable_async_codec_creation,
    MediaCodec::Factory* media_codec_factory,
    std::string* error_message)
    : JobOwner(job_queue),
      audio_stream_info_(audio_stream_info),
      sample_type_(GetSupportedSampleType()),
      enable_flush_during_seek_(enable_flush_during_seek),
      output_sample_rate_(audio_stream_info.samples_per_second),
      output_channel_count_(audio_stream_info.number_of_channels),
      drm_system_(static_cast<DrmSystem*>(drm_system)),
      media_codec_factory_(media_codec_factory
                               ? media_codec_factory
                               : &default_media_codec_factory_) {
  SB_CHECK(error_message);
  if (enable_async_codec_creation && !drm_system_) {
    // Configuring a codec for an encrypted stream needs the MediaCrypto of the
    // DRM system, so only clear streams are prepared on another thread.
    SB_LOG(INFO) << "Audio codec prepare started.";
    preparing_codec_ = true;
    prepare_started_at_us_ = CurrentMonotonicTime();
    prepare_thread_ = std::make_unique<PrepareThread>(this);
    // Must stay the last statement: PrepareCodec() uses this object.
    prepare_thread_->Start();
    return;
  }
  auto result = InitializeCodec();
  if (!result) {
    *error_message = result.error();
  }
}

MediaCodecAudioDecoder::~MediaCodecAudioDecoder() {
  if (prepare_thread_) {
    // PrepareCodec() uses this object, so wait for it before anything is
    // destroyed. The job it schedules is cancelled by ~JobOwner(), and a
    // prepared codec that has not been started is released with
    // |prepared_codec_|.
    prepare_thread_->Join();
  }
}

void MediaCodecAudioDecoder::Initialize(const OutputCB& output_cb,
                                        const ErrorCB& error_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(media_decoder_ || preparing_codec_ || error_before_initialize_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
  if (media_decoder_) {
    media_decoder_->Initialize(
        std::bind(&MediaCodecAudioDecoder::ReportError, this, _1, _2));
  }
  if (error_before_initialize_) {
    Schedule(std::bind(&MediaCodecAudioDecoder::ReportError, this,
                       kSbPlayerErrorDecode, *error_before_initialize_));
    error_before_initialize_.reset();
  }
}

void MediaCodecAudioDecoder::Decode(const InputBuffers& input_buffers,
                                    const ConsumedCB& consumed_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());
  SB_DCHECK(output_cb_);

  if (preparing_codec_) {
    // The caller writes no more input until |consumed_cb| is called, which
    // DecodeInternal() does once the codec is ready.
    parked_decodes_.push_back({input_buffers, consumed_cb});
    return;
  }
  if (!media_decoder_) {
    return;
  }
  DecodeInternal(input_buffers, consumed_cb);
}

void MediaCodecAudioDecoder::DecodeInternal(const InputBuffers& input_buffers,
                                            const ConsumedCB& consumed_cb) {
  audio_frame_discarder_.OnInputBuffers(input_buffers);

#if STARBOARD_ANDROID_SHARED_AUDIO_DECODER_VERBOSE
  for (const auto& input_buffer : input_buffers) {
    VERBOSE_MEDIA_LOG() << "T1: timestamp " << input_buffer->timestamp();
  }
#endif

  if (media_decoder_) {
    media_decoder_->WriteInputBuffers(input_buffers);
  }

  std::lock_guard lock(decoded_audios_mutex_);
  if (media_decoder_ &&
      (media_decoder_->GetNumberOfPendingInputs() + decoded_audios_.size() <=
       kMaxPendingWorkSize)) {
    Schedule(consumed_cb);
  } else {
    consumed_cb_ = consumed_cb;
  }
}

void MediaCodecAudioDecoder::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  if (preparing_codec_) {
    parked_end_of_stream_ = true;
    return;
  }

  if (media_decoder_) {
    media_decoder_->WriteEndOfStream();
  }
}

std::optional<DecodedAudio> MediaCodecAudioDecoder::Read(
    int* samples_per_second) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  std::optional<DecodedAudio> result;
  {
    std::lock_guard lock(decoded_audios_mutex_);
    SB_DCHECK(!decoded_audios_.empty());
    if (!decoded_audios_.empty()) {
      result = std::move(decoded_audios_.front());
      VERBOSE_MEDIA_LOG() << "T3: timestamp " << result->timestamp();
      decoded_audios_.pop();
    }
  }

  if (consumed_cb_) {
    Schedule(consumed_cb_);
    consumed_cb_ = nullptr;
  }
  *samples_per_second = audio_stream_info_.samples_per_second;
  return result;
}

void MediaCodecAudioDecoder::Reset() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  if (preparing_codec_) {
    // The codec being prepared has received no input and has not been started,
    // so it needs no flush and is used after the reset as it is. Only what was
    // written before the reset is dropped. The pending jobs are not cancelled:
    // the only one this object can have at this point is OnCodecPrepared(),
    // without which the decoder would never become ready.
    parked_decodes_.clear();
    parked_end_of_stream_ = false;
    audio_frame_discarder_.Reset();
    consumed_cb_ = nullptr;
    return;
  }

  // If fail to flush |media_decoder_| or |media_decoder_| is null, then
  // re-create |media_decoder_|.
  if (!enable_flush_during_seek_ || !media_decoder_ ||
      !media_decoder_->Flush()) {
    media_decoder_.reset();

    auto result = InitializeCodec();
    if (!result) {
      ReportError(
          kSbPlayerErrorDecode,
          "Failed to initialize audio decoder after reset: " + result.error());
    }
  }
  audio_frame_discarder_.Reset();

  consumed_cb_ = nullptr;

  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  CancelPendingJobs();
}

Result<void> MediaCodecAudioDecoder::InitializeCodec() {
  SB_DCHECK(!media_decoder_);
  auto result =
      MediaCodecDecoder::CreateForAudio(*media_codec_factory_, job_queue(),
                                        this, audio_stream_info_, drm_system_);
  if (result) {
    media_decoder_ = std::move(result.value());
    if (error_cb_) {
      media_decoder_->Initialize(
          std::bind(&MediaCodecAudioDecoder::ReportError, this, _1, _2));
    }
    return Success();
  }
  SB_LOG(ERROR) << "Failed to initialize audio decoder: " << result.error();
  return Failure(result.error());
}

void MediaCodecAudioDecoder::PrepareCodec() {
  // Runs on |prepare_thread_|. Apart from the factory call, it only hands the
  // result over: everything else, including the creation of
  // |media_decoder_|, has to happen on the thread of the job queue.
  auto prepared_codec =
      media_codec_factory_->PrepareAudioMediaCodec(audio_stream_info_);
  {
    std::lock_guard lock(prepare_mutex_);
    prepared_codec_ = std::move(prepared_codec);
    prepare_finished_at_us_ = CurrentMonotonicTime();
  }
  Schedule(std::bind(&MediaCodecAudioDecoder::OnCodecPrepared, this));
}

void MediaCodecAudioDecoder::OnCodecPrepared() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(preparing_codec_);
  SB_DCHECK(!media_decoder_);

  if (prepare_thread_) {
    prepare_thread_->Join();
    prepare_thread_.reset();
  }

  std::unique_ptr<MediaCodec::PreparedAudioCodec> prepared_codec;
  int64_t prepare_finished_at_us;
  {
    std::lock_guard lock(prepare_mutex_);
    prepared_codec = std::move(prepared_codec_);
    prepare_finished_at_us = prepare_finished_at_us_;
  }
  // From here on this object behaves as one created without
  // |kMediaAsyncAudioCodecCreation|, whatever happens below.
  preparing_codec_ = false;

  const int64_t start_started_at_us = CurrentMonotonicTime();
  if (prepared_codec) {
    auto result = MediaCodecDecoder::CreateForAudio(
        *media_codec_factory_, job_queue(), this, audio_stream_info_,
        std::move(prepared_codec));
    if (result) {
      media_decoder_ = std::move(result.value());
      if (error_cb_) {
        media_decoder_->Initialize(
            std::bind(&MediaCodecAudioDecoder::ReportError, this, _1, _2));
      }
    } else {
      SB_LOG(ERROR) << "Failed to start prepared audio codec: "
                    << result.error();
    }
  } else {
    SB_LOG(ERROR) << "Failed to prepare audio codec.";
  }

  const bool fallback = !media_decoder_;
  std::string fallback_error;
  if (fallback) {
    // Create the codec the way it is created without
    // |kMediaAsyncAudioCodecCreation|.
    auto result = InitializeCodec();
    if (!result) {
      fallback_error = result.error();
    }
  }
  const int64_t now_us = CurrentMonotonicTime();
  SB_LOG(INFO) << "Audio codec attached: start(msec)="
               << (now_us - start_started_at_us) / 1'000
               << ", waited_for_worker(msec)="
               << (start_started_at_us - prepare_finished_at_us) / 1'000
               << ", since_prepare_started(msec)="
               << (now_us - prepare_started_at_us_) / 1'000
               << ", parked_buffers=" << GetNumberOfParkedInputBuffers()
               << ", parked_end_of_stream=" << ToString(parked_end_of_stream_)
               << ", fallback=" << ToString(fallback)
               << ", ok=" << ToString(!!media_decoder_);

  auto parked_decodes = std::move(parked_decodes_);
  parked_decodes_.clear();
  const bool parked_end_of_stream = parked_end_of_stream_;
  parked_end_of_stream_ = false;

  if (!media_decoder_) {
    // Without the flag the decoder would not have been created, and its
    // creator would have reported the error. The parked input is dropped: its
    // |consumed_cb| is not called, as no more input can be accepted.
    const std::string error_message =
        "Failed to create audio decoder: " + fallback_error;
    if (error_cb_) {
      ReportError(kSbPlayerErrorDecode, error_message);
    } else {
      error_before_initialize_ = error_message;
    }
    return;
  }
  if (!fallback) {
    // Logged by the codec creation otherwise. Marks the end of the creation of
    // the audio codec in the log.
    SB_LOG(INFO) << "CreateAudioMediaCodec: audio_stream_info="
                 << audio_stream_info_;
  }

  for (const auto& parked_decode : parked_decodes) {
    DecodeInternal(parked_decode.input_buffers, parked_decode.consumed_cb);
  }
  if (parked_end_of_stream) {
    media_decoder_->WriteEndOfStream();
  }
}

size_t MediaCodecAudioDecoder::GetNumberOfParkedInputBuffers() const {
  size_t count = 0;
  for (const auto& parked_decode : parked_decodes_) {
    count += parked_decode.input_buffers.size();
  }
  return count;
}

void MediaCodecAudioDecoder::ProcessOutputBuffer(
    MediaCodec* media_codec_bridge,
    const DequeueOutputResult& dequeue_output_result,
    int /*number_of_pending_inputs*/) {
  SB_DCHECK(media_codec_bridge);
  SB_DCHECK(output_cb_);
  SB_DCHECK_GE(dequeue_output_result.index, 0);

  if (dequeue_output_result.num_bytes > 0) {
    void* address =
        media_codec_bridge->GetOutputBufferAddress(dequeue_output_result.index)
            .data();

    if (!address) {
      ReportError(kSbPlayerErrorDecode,
                  "Failed to process audio output buffer.");
      return;
    }

    int16_t* data = static_cast<int16_t*>(
        IncrementPointerByBytes(address, dequeue_output_result.offset));
    int size = dequeue_output_result.num_bytes;
    if (2 * audio_stream_info_.samples_per_second ==
        static_cast<uint32_t>(output_sample_rate_)) {
      // The audio is encoded using implicit HE-AAC, so the decoder outputs
      // twice as many frames per second as the audio sink, which has already
      // been created at the sample rate of the stream, expects.  Halve the
      // frame rate in place by averaging each pair of consecutive frames per
      // channel, so the audio sink can play it with the correct pitch.
      const size_t channels = audio_stream_info_.number_of_channels;
      SB_DCHECK_GT(channels, 0u);
      const size_t frames = size / sizeof(int16_t) / channels;
      for (size_t frame = 0; frame + 1 < frames; frame += 2) {
        for (size_t channel = 0; channel < channels; ++channel) {
          data[frame / 2 * channels + channel] =
              (static_cast<int32_t>(data[frame * channels + channel]) +
               static_cast<int32_t>(data[(frame + 1) * channels + channel])) /
              2;
        }
      }
      size = frames / 2 * channels * sizeof(int16_t);
    }

    DecodedAudio decoded_audio(
        audio_stream_info_.number_of_channels, sample_type_,
        dequeue_output_result.presentation_time_microseconds, size);

    memcpy(decoded_audio.data(), data, size);
    audio_frame_discarder_.AdjustForDiscardedDurations(
        audio_stream_info_.samples_per_second, &decoded_audio);

    {
      std::lock_guard lock(decoded_audios_mutex_);
      decoded_audios_.push(std::move(decoded_audio));
      VERBOSE_MEDIA_LOG() << "T2: timestamp "
                          << decoded_audios_.front().timestamp();
    }
    Schedule(output_cb_);
  }

  // BUFFER_FLAG_END_OF_STREAM may come with the last valid output buffer.
  if (dequeue_output_result.flags & MediaCodec::kBufferFlagEndOfStream) {
    {
      std::lock_guard lock(decoded_audios_mutex_);
      decoded_audios_.push(DecodedAudio::CreateEOSBuffer());
    }
    audio_frame_discarder_.OnDecodedAudioEndOfStream();
    Schedule(output_cb_);
  }

  media_codec_bridge->ReleaseOutputBuffer(dequeue_output_result.index, false);
}

void MediaCodecAudioDecoder::RefreshOutputFormat(
    MediaCodec* media_codec_bridge) {
  std::optional<AudioOutputFormatResult> output_format =
      media_codec_bridge->GetAudioOutputFormat();
  if (!output_format) {
    SB_LOG(ERROR) << "|getOutputFormat| failed";
    return;
  }
  output_sample_rate_ = output_format->sample_rate;
  output_channel_count_ = output_format->channel_count;
}

void MediaCodecAudioDecoder::ReportError(SbPlayerError error,
                                         const std::string& error_message) {
  SB_DCHECK(error_cb_);

  if (!error_cb_) {
    return;
  }

  error_cb_(kSbPlayerErrorDecode, error_message);
}

}  // namespace starboard

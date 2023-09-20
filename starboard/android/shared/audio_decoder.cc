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

#include "starboard/android/shared/audio_decoder.h"

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/memory.h"

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
namespace android {
namespace shared {

namespace {

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

AudioDecoder::AudioDecoder(const AudioStreamInfo& audio_stream_info,
                           SbDrmSystem drm_system)
    : audio_stream_info_(audio_stream_info),
      sample_type_(GetSupportedSampleType()),
      output_sample_rate_(audio_stream_info.samples_per_second),
      output_channel_count_(audio_stream_info.number_of_channels),
      drm_system_(static_cast<DrmSystem*>(drm_system)) {
  if (!InitializeCodec()) {
    SB_LOG(ERROR) << "Failed to initialize audio decoder.";
  }
}

AudioDecoder::~AudioDecoder() {}

void AudioDecoder::Initialize(const OutputCB& output_cb,
                              const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb);
  SB_DCHECK(!output_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(media_decoder_);

  output_cb_ = output_cb;
  error_cb_ = error_cb;
  if (media_decoder_) {
    media_decoder_->Initialize(
        std::bind(&AudioDecoder::ReportError, this, _1, _2));
  }
}

void AudioDecoder::Decode(const InputBuffers& input_buffers,
                          const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());
  SB_DCHECK(output_cb_);
  SB_DCHECK(media_decoder_);

  audio_frame_discarder_.OnInputBuffers(input_buffers);

#if STARBOARD_ANDROID_SHARED_AUDIO_DECODER_VERBOSE
  for (const auto& input_buffer : input_buffers) {
    VERBOSE_MEDIA_LOG() << "T1: timestamp " << input_buffer->timestamp();
  }
#endif

  if (media_decoder_) {
    media_decoder_->WriteInputBuffers(input_buffers);
  }

  ScopedLock lock(decoded_audios_mutex_);
  if (media_decoder_ &&
      (media_decoder_->GetNumberOfPendingTasks() + decoded_audios_.size() <=
       kMaxPendingWorkSize)) {
    Schedule(consumed_cb);
  } else {
    consumed_cb_ = consumed_cb;
  }
}

void AudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);
  SB_DCHECK(media_decoder_);

  if (media_decoder_) {
    media_decoder_->WriteEndOfStream();
  }
}

scoped_refptr<AudioDecoder::DecodedAudio> AudioDecoder::Read(
    int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  scoped_refptr<DecodedAudio> result;
  {
    starboard::ScopedLock lock(decoded_audios_mutex_);
    SB_DCHECK(!decoded_audios_.empty());
    if (!decoded_audios_.empty()) {
      result = decoded_audios_.front();
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

void AudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(output_cb_);

  media_decoder_.reset();
  audio_frame_discarder_.Reset();

  if (!InitializeCodec()) {
    // TODO: Communicate this failure to our clients somehow.
    SB_LOG(ERROR) << "Failed to initialize codec after reset.";
  }

  consumed_cb_ = nullptr;

  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }

  CancelPendingJobs();
}

bool AudioDecoder::InitializeCodec() {
  SB_DCHECK(!media_decoder_);
  media_decoder_.reset(new MediaDecoder(this, audio_stream_info_, drm_system_));
  if (media_decoder_->is_valid()) {
    if (error_cb_) {
      media_decoder_->Initialize(
          std::bind(&AudioDecoder::ReportError, this, _1, _2));
    }
    return true;
  }
  media_decoder_.reset();
  return false;
}

void AudioDecoder::ProcessOutputBuffer(
    MediaCodecBridge* media_codec_bridge,
    const DequeueOutputResult& dequeue_output_result) {
  SB_DCHECK(media_codec_bridge);
  SB_DCHECK(output_cb_);
  SB_DCHECK(dequeue_output_result.index >= 0);

  if (dequeue_output_result.num_bytes > 0) {
    ScopedJavaByteBuffer byte_buffer(
        media_codec_bridge->GetOutputBuffer(dequeue_output_result.index));

    if (byte_buffer.IsNull()) {
      ReportError(kSbPlayerErrorDecode,
                  "Failed to process audio output buffer.");
      return;
    }

    int16_t* data = static_cast<int16_t*>(IncrementPointerByBytes(
        byte_buffer.address(), dequeue_output_result.offset));
    int size = dequeue_output_result.num_bytes;
    if (2 * audio_stream_info_.samples_per_second == output_sample_rate_) {
      // The audio is encoded using implicit HE-AAC.  As the audio sink has
      // been created already we try to down-mix the decoded data to half of
      // its channels so the audio sink can play it with the correct pitch.
      for (int i = 0; i < size / sizeof(int16_t); i++) {
        data[i / 2] = (static_cast<int32_t>(data[i]) +
                       static_cast<int32_t>(data[i + 1]) / 2);
      }
      size /= 2;
    }

    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        audio_stream_info_.number_of_channels, sample_type_,
        kSbMediaAudioFrameStorageTypeInterleaved,
        dequeue_output_result.presentation_time_microseconds, size);

    memcpy(decoded_audio->data(), data, size);
    audio_frame_discarder_.AdjustForDiscardedDurations(
        audio_stream_info_.samples_per_second, &decoded_audio);

    {
      starboard::ScopedLock lock(decoded_audios_mutex_);
      decoded_audios_.push(decoded_audio);
      VERBOSE_MEDIA_LOG() << "T2: timestamp "
                          << decoded_audios_.front()->timestamp();
    }
    Schedule(output_cb_);
  }

  // BUFFER_FLAG_END_OF_STREAM may come with the last valid output buffer.
  if (dequeue_output_result.flags & BUFFER_FLAG_END_OF_STREAM) {
    {
      starboard::ScopedLock lock(decoded_audios_mutex_);
      decoded_audios_.push(new DecodedAudio());
    }
    audio_frame_discarder_.OnDecodedAudioEndOfStream();
    Schedule(output_cb_);
  }

  media_codec_bridge->ReleaseOutputBuffer(dequeue_output_result.index, false);
}

void AudioDecoder::RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) {
  AudioOutputFormatResult output_format =
      media_codec_bridge->GetAudioOutputFormat();
  if (output_format.status == MEDIA_CODEC_ERROR) {
    SB_LOG(ERROR) << "|getOutputFormat| failed";
    return;
  }
  output_sample_rate_ = output_format.sample_rate;
  output_channel_count_ = output_format.channel_count;
}

void AudioDecoder::ReportError(SbPlayerError error,
                               const std::string& error_message) {
  SB_DCHECK(error_cb_);

  if (!error_cb_) {
    return;
  }

  error_cb_(kSbPlayerErrorDecode, error_message);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

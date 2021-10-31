// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/stub_audio_decoder.h"

#include "starboard/audio_sink.h"
#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

SbMediaAudioSampleType GetSupportedSampleType() {
  if (SbAudioSinkIsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32)) {
    return kSbMediaAudioSampleTypeFloat32;
  }
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(
      kSbMediaAudioSampleTypeInt16Deprecated));
  return kSbMediaAudioSampleTypeInt16Deprecated;
}

}  // namespace

StubAudioDecoder::StubAudioDecoder(
    SbMediaAudioCodec audio_codec,
    const SbMediaAudioSampleInfo& audio_sample_info)
    : sample_type_(GetSupportedSampleType()),
      audio_codec_(audio_codec),
      audio_sample_info_(audio_sample_info) {}

void StubAudioDecoder::Initialize(const OutputCB& output_cb,
                                  const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());

  output_cb_ = output_cb;
  error_cb_ = error_cb;
}

void StubAudioDecoder::Decode(const scoped_refptr<InputBuffer>& input_buffer,
                              const ConsumedCB& consumed_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("stub_audio_decoder"));
  }
  decoder_thread_->job_queue()->Schedule(std::bind(
      &StubAudioDecoder::DecodeOneBuffer, this, input_buffer, consumed_cb));
}

void StubAudioDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  if (decoder_thread_) {
    decoder_thread_->job_queue()->Schedule(
        std::bind(&StubAudioDecoder::DecodeEndOfStream, this));
    return;
  }
  decoded_audios_.push(new DecodedAudio());
  output_cb_();
}

scoped_refptr<DecodedAudio> StubAudioDecoder::Read(int* samples_per_second) {
  SB_DCHECK(BelongsToCurrentThread());

  *samples_per_second = audio_sample_info_.samples_per_second;
  ScopedLock lock(decoded_audios_mutex_);
  if (decoded_audios_.empty()) {
    return scoped_refptr<DecodedAudio>();
  }
  auto result = decoded_audios_.front();
  decoded_audios_.pop();
  return result;
}

void StubAudioDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  decoder_thread_.reset();
  last_input_buffer_ = NULL;
  total_input_count_ = 0;
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  CancelPendingJobs();
}

void StubAudioDecoder::DecodeOneBuffer(
    const scoped_refptr<InputBuffer>& input_buffer,
    const ConsumedCB& consumed_cb) {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  // Values to represent what kind of dummy audio to fill the decoded audio
  // we produce with.
  enum FillType {
    kSilence,
    kWave,
  };
  // Can be set locally to fill with different types.
  const FillType fill_type = kSilence;
  const int kMaxInputBeforeMultipleDecodedAudios = 4;

  if (last_input_buffer_) {
    SbTime total_output_duration =
        input_buffer->timestamp() - last_input_buffer_->timestamp();
    if (total_output_duration < 0) {
      SB_DLOG(ERROR) << "Total output duration " << total_output_duration
                     << " is invalid.";
      error_cb_(kSbPlayerErrorDecode, "Total output duration is less than 0.");
      return;
    }
    size_t sample_size =
        sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated ? 2 : 4;
    size_t total_frame_size = total_output_duration *
                              audio_sample_info_.samples_per_second /
                              kSbTimeSecond;
    if (audio_codec_ == kSbMediaAudioCodecAac) {
      // Frame size for AAC is fixed at 1024 samples.
      total_frame_size = 1024;
    }

    // Send 3 decoded audio objects on every 5th call to DecodeOneBuffer().
    int num_decoded_audio_objects =
        total_input_count_ % kMaxInputBeforeMultipleDecodedAudios == 0 ? 3 : 1;
    size_t output_frame_size = total_frame_size / num_decoded_audio_objects;
    size_t output_byte_size =
        output_frame_size * sample_size * audio_sample_info_.number_of_channels;

    for (int i = 0; i < num_decoded_audio_objects; ++i) {
      SbTime timestamp =
          last_input_buffer_->timestamp() +
          ((total_output_duration / num_decoded_audio_objects) * i);
      // Calculate the output frame size of the last decoded audio object, which
      // may be larger than the rest.
      if (i == num_decoded_audio_objects - 1 && num_decoded_audio_objects > 1) {
        output_frame_size = total_frame_size -
                            (num_decoded_audio_objects - 1) * output_frame_size;
        output_byte_size = output_frame_size * sample_size *
                           audio_sample_info_.number_of_channels;
      }
      scoped_refptr<DecodedAudio> decoded_audio(
          new DecodedAudio(audio_sample_info_.number_of_channels, sample_type_,
                           kSbMediaAudioFrameStorageTypeInterleaved, timestamp,
                           output_byte_size));

      if (fill_type == kSilence) {
        memset(decoded_audio.get()->buffer(), 0, output_byte_size);
      } else {
        SB_DCHECK(fill_type == kWave);
        for (int j = 0; j < output_byte_size / sample_size; ++j) {
          if (sample_size == 2) {
            *(reinterpret_cast<int16_t*>(decoded_audio.get()->buffer()) + j) =
                j;
          } else {
            SB_DCHECK(sample_size == 4);
            *(reinterpret_cast<float*>(decoded_audio.get()->buffer()) + j) =
                ((j % 1024) - 512) / 512.0f;
          }
        }
      }
      ScopedLock lock(decoded_audios_mutex_);
      decoded_audios_.push(decoded_audio);
      decoder_thread_->job_queue()->Schedule(output_cb_);
    }
  }
  decoder_thread_->job_queue()->Schedule(consumed_cb);
  last_input_buffer_ = input_buffer;
  total_input_count_++;
}

void StubAudioDecoder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  if (last_input_buffer_) {
    // There won't be a next pts, so just guess that the decoded size is
    // 4 times the encoded size.
    size_t fake_size = 4 * last_input_buffer_->size();
    size_t sample_size =
        sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated ? 2 : 4;
    fake_size -=
        fake_size % (sample_size * audio_sample_info_.number_of_channels);
    if (audio_codec_ == kSbMediaAudioCodecAac) {
      // Frame size for AAC is fixed at 1024, so fake the output size such that
      // number of frames matches up.
      fake_size = sample_size * audio_sample_info_.number_of_channels * 1024;
    }
    ScopedLock lock(decoded_audios_mutex_);
    decoded_audios_.push(
        new DecodedAudio(audio_sample_info_.number_of_channels, sample_type_,
                         kSbMediaAudioFrameStorageTypeInterleaved,
                         last_input_buffer_->timestamp(), fake_size));
    decoder_thread_->job_queue()->Schedule(output_cb_);
  }
  ScopedLock lock(decoded_audios_mutex_);
  decoded_audios_.push(new DecodedAudio());
  decoder_thread_->job_queue()->Schedule(output_cb_);
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

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
      audio_sample_info_(audio_sample_info),
      stream_ended_(false) {}

void StubAudioDecoder::Initialize(const OutputCB& output_cb,
                                  const ErrorCB& error_cb) {
  SB_UNREFERENCED_PARAMETER(error_cb);
  output_cb_ = output_cb;
}

void StubAudioDecoder::Decode(const scoped_refptr<InputBuffer>& input_buffer,
                              const ConsumedCB& consumed_cb) {
  SB_DCHECK(input_buffer);

  // Values to represent what kind of dummy audio to fill the decoded audio
  // we produce with.
  enum FillType {
    kSilence,
    kWave,
  };
  // Can be set locally to fill with different types.
  const FillType fill_type = kSilence;

  if (last_input_buffer_) {
    SbTime diff = input_buffer->timestamp() - last_input_buffer_->timestamp();
    SB_DCHECK(diff >= 0);
    size_t sample_size =
        GetSampleType() == kSbMediaAudioSampleTypeInt16Deprecated ? 2 : 4;
    size_t size = diff * GetSamplesPerSecond() * sample_size *
                  audio_sample_info_.number_of_channels / kSbTimeSecond;
    size -= size % (sample_size * audio_sample_info_.number_of_channels);
    if (audio_codec_ == kSbMediaAudioCodecAac) {
      // Frame size for AAC is fixed at 1024, so fake the output size such that
      // number of frames matches up.
      size = sample_size * audio_sample_info_.number_of_channels * 1024;
    }

    decoded_audios_.push(new DecodedAudio(
        audio_sample_info_.number_of_channels, GetSampleType(),
        GetStorageType(), last_input_buffer_->timestamp(), size));

    if (fill_type == kSilence) {
      SbMemorySet(decoded_audios_.back()->buffer(), 0, size);
    } else {
      SB_DCHECK(fill_type == kWave);
      for (int i = 0; i < size / sample_size; ++i) {
        if (sample_size == 2) {
          *(reinterpret_cast<int16_t*>(decoded_audios_.back()->buffer()) + i) =
              i;
        } else {
          SB_DCHECK(sample_size == 4);
          *(reinterpret_cast<float*>(decoded_audios_.back()->buffer()) + i) =
              ((i % 1024) - 512) / 512.0f;
        }
      }
    }
    Schedule(output_cb_);
  }
  Schedule(consumed_cb);
  last_input_buffer_ = input_buffer;
}

void StubAudioDecoder::WriteEndOfStream() {
  if (last_input_buffer_) {
    // There won't be a next pts, so just guess that the decoded size is
    // 4 times the encoded size.
    size_t fake_size = 4 * last_input_buffer_->size();
    size_t sample_size =
        GetSampleType() == kSbMediaAudioSampleTypeInt16Deprecated ? 2 : 4;
    fake_size -=
        fake_size % (sample_size * audio_sample_info_.number_of_channels);
    if (audio_codec_ == kSbMediaAudioCodecAac) {
      // Frame size for AAC is fixed at 1024, so fake the output size such that
      // number of frames matches up.
      fake_size = sample_size * audio_sample_info_.number_of_channels * 1024;
    }
    decoded_audios_.push(new DecodedAudio(
        audio_sample_info_.number_of_channels, GetSampleType(),
        GetStorageType(), last_input_buffer_->timestamp(), fake_size));
    Schedule(output_cb_);
  }
  decoded_audios_.push(new DecodedAudio());
  stream_ended_ = true;
  Schedule(output_cb_);
}

scoped_refptr<DecodedAudio> StubAudioDecoder::Read() {
  scoped_refptr<DecodedAudio> result;
  if (!decoded_audios_.empty()) {
    result = decoded_audios_.front();
    decoded_audios_.pop();
  }
  return result;
}

void StubAudioDecoder::Reset() {
  while (!decoded_audios_.empty()) {
    decoded_audios_.pop();
  }
  stream_ended_ = false;
  last_input_buffer_ = NULL;

  CancelPendingJobs();
}
SbMediaAudioSampleType StubAudioDecoder::GetSampleType() const {
  return sample_type_;
}
SbMediaAudioFrameStorageType StubAudioDecoder::GetStorageType() const {
  return kSbMediaAudioFrameStorageTypeInterleaved;
}
int StubAudioDecoder::GetSamplesPerSecond() const {
  return audio_sample_info_.samples_per_second;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

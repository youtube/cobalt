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

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/shared/starboard/player/filter/audio_resampler.h"
#include "starboard/shared/starboard/player/filter/interleaved_sinc_resampler.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

// CPU based simple AudioResampler implementation.  Note that it currently only
// supports resample audio between same storage type and same channels but with
// different sample types.
class AudioResamplerImpl : public AudioResampler {
 public:
  AudioResamplerImpl(SbMediaAudioSampleType source_sample_type,
                     SbMediaAudioFrameStorageType source_storage_type,
                     int source_sample_rate,
                     SbMediaAudioSampleType destination_sample_type,
                     SbMediaAudioFrameStorageType destination_storage_type,
                     int destination_sample_rate,
                     int channels)
      : source_sample_type_(source_sample_type),
        source_storage_type_(source_storage_type),
        destination_sample_type_(destination_sample_type),
        destination_storage_type_(destination_storage_type),
        interleaved_resampler_(static_cast<double>(source_sample_rate) /
                                   static_cast<double>(destination_sample_rate),
                               channels) {}

  scoped_refptr<DecodedAudio> Resample(
      const scoped_refptr<DecodedAudio>& buffer) override;

  scoped_refptr<DecodedAudio> WriteEndOfStream() override;

 private:
  InterleavedSincResampler interleaved_resampler_;

  SbMediaAudioSampleType source_sample_type_;
  SbMediaAudioFrameStorageType source_storage_type_;
  SbMediaAudioSampleType destination_sample_type_;
  SbMediaAudioFrameStorageType destination_storage_type_;

  size_t frames_to_resample_ = 0;
  size_t frames_resampled_ = 0;
};

}  // namespace

// static
scoped_ptr<AudioResampler> AudioResampler::Create(
    SbMediaAudioSampleType source_sample_type,
    SbMediaAudioFrameStorageType source_storage_type,
    int source_sample_rate,
    SbMediaAudioSampleType destination_sample_type,
    SbMediaAudioFrameStorageType destination_storage_type,
    int destination_sample_rate,
    int channels) {
  return scoped_ptr<AudioResampler>(new AudioResamplerImpl(
      source_sample_type, source_storage_type, source_sample_rate,
      destination_sample_type, destination_storage_type,
      destination_sample_rate, channels));
}

scoped_refptr<DecodedAudio> AudioResamplerImpl::WriteEndOfStream() {
  double sample_rate_ratio = interleaved_resampler_.GetSampleRateRatio();
  int out_num_of_frames =
      round(frames_to_resample_ / sample_rate_ratio) - frames_resampled_;
  if (out_num_of_frames > 0) {
    interleaved_resampler_.QueueBuffer(nullptr, 0);
    int channels = interleaved_resampler_.channels();
    int resampled_audio_size = out_num_of_frames * channels * sizeof(float);
    scoped_refptr<DecodedAudio> resampled_audio = new DecodedAudio(
        channels, kSbMediaAudioSampleTypeFloat32,
        kSbMediaAudioFrameStorageTypeInterleaved, 0, resampled_audio_size);

    float* dst = reinterpret_cast<float*>(resampled_audio->buffer());
    interleaved_resampler_.Resample(dst, out_num_of_frames);

    resampled_audio->SwitchFormatTo(destination_sample_type_,
                                    destination_storage_type_);
    return resampled_audio;
  }

  return new DecodedAudio;
}

scoped_refptr<DecodedAudio> AudioResamplerImpl::Resample(
    const scoped_refptr<DecodedAudio>& audio_data) {
  SB_DCHECK(audio_data->channels() == interleaved_resampler_.channels());

  // It does nothing if source sample type is float and source storage type is
  // interleaved.
  audio_data->SwitchFormatTo(kSbMediaAudioSampleTypeFloat32,
                             kSbMediaAudioFrameStorageTypeInterleaved);

  int num_of_frames = audio_data->frames();
  frames_to_resample_ += num_of_frames;
  int channels = audio_data->channels();
  int out_num_of_frames = static_cast<int>(
      ceil(num_of_frames / interleaved_resampler_.GetSampleRateRatio()));

  int resampled_audio_size = out_num_of_frames * channels * sizeof(float);

  scoped_refptr<DecodedAudio> resampled_audio = nullptr;

  float* samples = reinterpret_cast<float*>(audio_data->buffer());
  interleaved_resampler_.QueueBuffer(samples, num_of_frames);

  if (interleaved_resampler_.HasEnoughData(out_num_of_frames)) {
    resampled_audio =
        new DecodedAudio(channels, kSbMediaAudioSampleTypeFloat32,
                         kSbMediaAudioFrameStorageTypeInterleaved,
                         audio_data->timestamp(), resampled_audio_size);
    float* dst = reinterpret_cast<float*>(resampled_audio->buffer());
    interleaved_resampler_.Resample(dst, out_num_of_frames);
    frames_resampled_ += out_num_of_frames;

    resampled_audio->SwitchFormatTo(destination_sample_type_,
                                    destination_storage_type_);
  }

  return resampled_audio;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

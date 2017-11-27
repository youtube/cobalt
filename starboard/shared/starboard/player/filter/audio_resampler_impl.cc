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

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/player/filter/audio_resampler.h"

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
                     SbMediaAudioSampleType destination_sample_type,
                     SbMediaAudioFrameStorageType destination_storage_type,
                     int channels)
      : source_sample_type_(source_sample_type),
        source_storage_type_(source_storage_type),
        destination_sample_type_(destination_sample_type),
        destination_storage_type_(destination_storage_type),
        channels_(channels) {}

  scoped_refptr<DecodedAudio> Resample(
      const scoped_refptr<DecodedAudio>& audio_data) override {
    SB_DCHECK(audio_data->sample_type() == source_sample_type_);
    SB_DCHECK(audio_data->storage_type() == source_storage_type_);
    SB_DCHECK(audio_data->channels() == channels_);
    audio_data->SwitchFormatTo(destination_sample_type_,
                               destination_storage_type_);
    return audio_data;
  }

  scoped_refptr<DecodedAudio> WriteEndOfStream() override {
    return new DecodedAudio;
  }

 private:
  SbMediaAudioSampleType source_sample_type_;
  SbMediaAudioFrameStorageType source_storage_type_;
  SbMediaAudioSampleType destination_sample_type_;
  SbMediaAudioFrameStorageType destination_storage_type_;
  int channels_;
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
  SB_DCHECK(source_sample_rate == destination_sample_rate);
  return scoped_ptr<AudioResampler>(new AudioResamplerImpl(
      source_sample_type, source_storage_type, destination_sample_type,
      destination_storage_type, channels));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

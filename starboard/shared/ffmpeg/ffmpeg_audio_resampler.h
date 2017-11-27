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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_RESAMPLER_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_RESAMPLER_H_

#include <queue>

#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_resampler.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class AudioResampler : public starboard::player::filter::AudioResampler {
 public:
  AudioResampler(SbMediaAudioSampleType source_sample_type,
                 SbMediaAudioFrameStorageType source_storage_type,
                 int source_sample_rate,
                 SbMediaAudioSampleType destination_sample_type,
                 SbMediaAudioFrameStorageType destination_storage_type,
                 int destination_sample_rate,
                 int channels);
  ~AudioResampler() override;

  scoped_refptr<DecodedAudio> Resample(
      const scoped_refptr<DecodedAudio>& audio_data) override;
  scoped_refptr<DecodedAudio> WriteEndOfStream() override;

 private:
  scoped_refptr<DecodedAudio> RetrieveOutput();

  shared::starboard::ThreadChecker thread_checker_;

  SbMediaAudioSampleType source_sample_type_;
  SbMediaAudioFrameStorageType source_storage_type_;
  SbMediaAudioSampleType destination_sample_type_;
  SbMediaAudioFrameStorageType destination_storage_type_;
  int destination_sample_rate_;  // Used for timestamp adjustment.
  int channels_;
  SbMediaTime start_pts_;
  int samples_returned_;
  bool eos_reached_;
  AVAudioResampleContext* context_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_RESAMPLER_H_

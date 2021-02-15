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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RESAMPLER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RESAMPLER_H_

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// Classes inherited from this interface can convert input audio samples in one
// sample and storage type into another sample and storage type.  For example,
// it can convert planar int16 samples into interleaved float32 samples.
// All functions (including Create() and the dtor) of the class should be called
// on the same thread as the JobQueue passed in the Create() function.
// It doesn't have a function to reset its internal state so during a seek the
// user of this class should destroy and recreate it.
class AudioResampler {
 public:
  typedef ::starboard::shared::starboard::player::DecodedAudio DecodedAudio;

  virtual ~AudioResampler() {}

  // Write frames to the AudioResampler.  The format of the frames is determined
  // by the input formats passed to Create().
  virtual scoped_refptr<DecodedAudio> Resample(
      const scoped_refptr<DecodedAudio>& audio_data) = 0;

  // Signal that the last audio input frame has been written.  The resampler
  // should allow for reading of any audio data inside its internal cache.  The
  // caller should continue call Read() after calling this function until Read()
  // returns EOS.
  virtual scoped_refptr<DecodedAudio> WriteEndOfStream() = 0;

  // Create an AudioResampler that takes input specified by |source_*| and
  // produce output specified by |destination_*|.  The input and output have to
  // have the same number of channels which is specified in |channels|.
  // The |output_cb| will be called whenever there is resampled data ready.  It
  // is always called asynchronously on |job_queue| and then the user of
  // AudioResampler can call Read() to read the next chunk of resampled data.
  static scoped_ptr<AudioResampler> Create(
      SbMediaAudioSampleType source_sample_type,
      SbMediaAudioFrameStorageType source_storage_type,
      int source_sample_rate,
      SbMediaAudioSampleType destination_sample_type,
      SbMediaAudioFrameStorageType destination_storage_type,
      int destination_sample_rate,
      int channels);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RESAMPLER_H_

// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_DECODER_PASSTHROUGH_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_DECODER_PASSTHROUGH_H_

#include <queue>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/types.h"

namespace starboard {
namespace android {
namespace shared {

// This class simply creates a DecodedAudio object from the InputBuffer passed
// in, without actually decoding the input audio.  It can be used in situations
// (like passthrough playbacks) where an AudioDecoder has to be used, but is
// expected to not alter the input and pass it to the renderer as is.
class AudioDecoderPassthrough
    : public ::starboard::shared::starboard::player::filter::AudioDecoder {
 public:
  explicit AudioDecoderPassthrough(int samples_per_second)
      : samples_per_second_(samples_per_second) {}

  // AudioDecoder methods.
  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(!output_cb_);
    SB_DCHECK(output_cb);

    output_cb_ = output_cb;
  }

  void Decode(const scoped_refptr<InputBuffer>& input_buffer,
              const ConsumedCB& consumed_cb) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(input_buffer);
    SB_DCHECK(consumed_cb);
    SB_DCHECK(output_cb_);

    // TODO: |decoded_audio| is used as a buffer to store raw, encoded audio
    //       here, which isn't aligned to its intended usage.  The code won't
    //       break as its channel is explicitly set to 1, and the ctor of
    //       DecodedAudio doesn't check whether the buffer size is a multiple of
    //       the sample size.
    //       We should revisit this once |DecodedAudio| is used by passthrough
    //       mode on more platforms.
    const int kChannels = 1;
    scoped_refptr<DecodedAudio> decoded_audio =
        new DecodedAudio(kChannels, kSbMediaAudioSampleTypeInt16Deprecated,
                         kSbMediaAudioFrameStorageTypePlanar,
                         input_buffer->timestamp(), input_buffer->size());
    memcpy(decoded_audio->buffer(), input_buffer->data(), input_buffer->size());
    decoded_audios_.push(decoded_audio);

    consumed_cb();
    output_cb_();
  }

  void WriteEndOfStream() override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(output_cb_);

    decoded_audios_.push(new DecodedAudio);
    output_cb_();
  }

  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(samples_per_second);
    SB_DCHECK(!decoded_audios_.empty());

    *samples_per_second = samples_per_second_;

    auto decoded_audio = decoded_audios_.front();
    decoded_audios_.pop();
    return decoded_audio;
  }

  void Reset() override {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    decoded_audios_ = std::queue<scoped_refptr<DecodedAudio>>();  // Clear
  }

 private:
  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  const int samples_per_second_;
  OutputCB output_cb_;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_DECODER_PASSTHROUGH_H_

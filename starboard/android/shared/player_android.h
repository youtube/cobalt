// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_PLAYER_ANDROID_H_
#define STARBOARD_ANDROID_SHARED_PLAYER_ANDROID_H_

#include <memory>
#include <utility>

#include "starboard/android/shared/video_decoder_experimental_features.h"
#include "starboard/shared/starboard/player/player_internal.h"
#include "starboard/shared/starboard/player/player_worker.h"

namespace starboard::android::shared {

class SbPlayerAndroid
    : public ::starboard::shared::starboard::player::SbPlayerPrivateImpl {
 public:
  static SbPlayerAndroid* Create(
      SbMediaAudioCodec audio_codec,
      SbMediaVideoCodec video_codec,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      std::unique_ptr<
          ::starboard::shared::starboard::player::PlayerWorker::Handler>
          player_worker_handler,
      const VideoDecoderExperimentalFeatures& experimental_features) {
    auto player = std::unique_ptr<SbPlayerAndroid>(new SbPlayerAndroid(
        audio_codec, video_codec, sample_deallocate_func, decoder_status_func,
        player_status_func, player_error_func, context,
        std::move(player_worker_handler), experimental_features));
    if (!player->worker_) {
      return nullptr;
    }
    return player.release();
  }

  const VideoDecoderExperimentalFeatures& experimental_features() const {
    return experimental_features_;
  }

 private:
  SbPlayerAndroid(
      SbMediaAudioCodec audio_codec,
      SbMediaVideoCodec video_codec,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      std::unique_ptr<
          ::starboard::shared::starboard::player::PlayerWorker::Handler>
          player_worker_handler,
      const VideoDecoderExperimentalFeatures& experimental_features)
      : SbPlayerPrivateImpl(audio_codec,
                            video_codec,
                            sample_deallocate_func,
                            decoder_status_func,
                            player_status_func,
                            player_error_func,
                            context,
                            std::move(player_worker_handler)),
        experimental_features_(experimental_features) {}

  const VideoDecoderExperimentalFeatures experimental_features_;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_PLAYER_ANDROID_H_

// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_H_
#define STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_H_

#include <memory>

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"
#include "starboard/media.h"
#include "starboard/player.h"

namespace starboard {
namespace android {
namespace shared {

class ExoPlayer final {
 public:
  static SbPlayerPrivate* CreateInstance(
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func);
  static ExoPlayer* GetExoPlayerForSbPlayer(SbPlayer player);

  ~ExoPlayer();

  void Seek(int64_t seek_to_timestamp, int ticket) const;
  void WriteSamples(SbMediaType sample_type,
                    const SbPlayerSampleInfo* sample_infos,
                    int number_of_sample_infos) const;
  void WriteEndOfStream(SbMediaType stream_type) const;
  void SetBounds(int z_index, int x, int y, int width, int height) const;
  bool SetPlaybackRate(double playback_rate) const;
  void SetVolume(double volume) const;
  void GetInfo(SbPlayerInfo* out_player_info) const;

 private:
  ExoPlayer(SbPlayerDeallocateSampleFunc sample_deallocate_func,
            SbPlayerDecoderStatusFunc decoder_status_func,
            SbPlayerStatusFunc player_status_func,
            SbPlayerErrorFunc player_error_func);

  std::unique_ptr<ExoPlayerBridge> bridge_;

  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  SbPlayerDecoderStatusFunc decoder_status_func_;
  SbPlayerStatusFunc player_status_func_;
  SbPlayerErrorFunc player_error_func_;

  bool is_playing_ = false;

  // A reference to the ExoPlayer as an SbPlayer.
  const SbPlayer player_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_H_

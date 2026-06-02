//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_PLAYER_PLAYER_INTERNAL_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_PLAYER_PLAYER_INTERNAL_H_

#include <memory>

#include "starboard/common/rect.h"
#include "starboard/player.h"

namespace starboard {
struct SB_EXPORT Player {
  virtual ~Player() {}
  static int MaxNumberOfSamplesPerWrite();
  virtual void MarkEOS(SbMediaType stream_type) = 0;
  virtual void WriteSample(SbMediaType sample_type,
                           const SbPlayerSampleInfo* sample_infos,
                           int number_of_sample_infos) = 0;
  virtual void SetVolume(double volume) = 0;
  virtual void Seek(int64_t seek_to_timestamp, int ticket) = 0;
  virtual bool SetRate(double rate) = 0;
  virtual void GetInfo(SbPlayerInfo* info) = 0;
  virtual void SetBounds(int zindex, const Rect& rect) = 0;
};

void ForceStop();
void AudioConfigurationChanged();

}  // namespace starboard

struct SbPlayerPrivate {
  SbPlayerPrivate(SbWindow window,
                  SbMediaVideoCodec video_codec,
                  SbMediaAudioCodec audio_codec,
                  SbDrmSystem drm_system,
                  const SbMediaAudioStreamInfo& audio_info,
                  const char* max_video_capabilities,
                  SbPlayerDeallocateSampleFunc sample_deallocate_func,
                  SbPlayerDecoderStatusFunc decoder_status_func,
                  SbPlayerStatusFunc player_status_func,
                  SbPlayerErrorFunc player_error_func,
                  void* context,
                  SbPlayerOutputMode output_mode,
                  SbDecodeTargetGraphicsContextProvider* provider);
  ~SbPlayerPrivate() {}

  int MaxNumberOfSamplesPerWrite() const {
    return starboard::Player::MaxNumberOfSamplesPerWrite();
  }

  std::unique_ptr<starboard::Player> player_;
};

#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_PLAYER_PLAYER_INTERNAL_H_

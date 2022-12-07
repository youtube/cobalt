// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_SBPLAYER_INTERFACE_H_
#define COBALT_MEDIA_BASE_SBPLAYER_INTERFACE_H_

#include "starboard/player.h"

#if SB_HAS(PLAYER_WITH_URL)
#include SB_URL_PLAYER_INCLUDE_PATH
#endif  // SB_HAS(PLAYER_WITH_URL)

namespace cobalt {
namespace media {

class SbPlayerInterface {
 public:
  virtual ~SbPlayerInterface() {}

  virtual SbPlayer Create(
      SbWindow window, const SbPlayerCreationParam* creation_param,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func, void* context,
      SbDecodeTargetGraphicsContextProvider* context_provider) = 0;
  virtual SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) = 0;
  virtual void Destroy(SbPlayer player) = 0;
  virtual void Seek(SbPlayer player, SbTime seek_to_timestamp, int ticket) = 0;
  virtual void WriteSample(SbPlayer player, SbMediaType sample_type,
                           const SbPlayerSampleInfo* sample_infos,
                           int number_of_sample_infos) = 0;
  virtual int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                                SbMediaType sample_type) = 0;
  virtual void WriteEndOfStream(SbPlayer player, SbMediaType stream_type) = 0;
  virtual void SetBounds(SbPlayer player, int z_index, int x, int y, int width,
                         int height) = 0;
  virtual bool SetPlaybackRate(SbPlayer player, double playback_rate) = 0;
  virtual void SetVolume(SbPlayer player, double volume) = 0;
  virtual void GetInfo(SbPlayer player, SbPlayerInfo2* out_player_info2) = 0;
  virtual SbDecodeTarget GetCurrentFrame(SbPlayer player) = 0;

#if SB_HAS(PLAYER_WITH_URL)
  virtual SbPlayer CreateUrlPlayer(const char* url, SbWindow window,
                                   SbPlayerStatusFunc player_status_func,
                                   SbPlayerEncryptedMediaInitDataEncounteredCB
                                       encrypted_media_init_data_encountered_cb,
                                   SbPlayerErrorFunc player_error_func,
                                   void* context) = 0;
  virtual void SetUrlPlayerDrmSystem(SbPlayer player,
                                     SbDrmSystem drm_system) = 0;
  virtual bool GetUrlPlayerOutputModeSupported(
      SbPlayerOutputMode output_mode) = 0;
  virtual void GetUrlPlayerExtraInfo(
      SbPlayer player, SbUrlPlayerExtraInfo* out_url_player_info) = 0;
#endif  // SB_HAS(PLAYER_WITH_URL)
};

class DefaultSbPlayerInterface final : public SbPlayerInterface {
 public:
  SbPlayer Create(
      SbWindow window, const SbPlayerCreationParam* creation_param,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func, void* context,
      SbDecodeTargetGraphicsContextProvider* context_provider) override;
  SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) override;
  void Destroy(SbPlayer player) override;
  void Seek(SbPlayer player, SbTime seek_to_timestamp, int ticket) override;
  void WriteSample(SbPlayer player, SbMediaType sample_type,
                   const SbPlayerSampleInfo* sample_infos,
                   int number_of_sample_infos) override;
  int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                        SbMediaType sample_type) override;
  void WriteEndOfStream(SbPlayer player, SbMediaType stream_type) override;
  void SetBounds(SbPlayer player, int z_index, int x, int y, int width,
                 int height) override;
  bool SetPlaybackRate(SbPlayer player, double playback_rate) override;
  void SetVolume(SbPlayer player, double volume) override;
  void GetInfo(SbPlayer player, SbPlayerInfo2* out_player_info2) override;
  SbDecodeTarget GetCurrentFrame(SbPlayer player) override;

#if SB_HAS(PLAYER_WITH_URL)
  SbPlayer CreateUrlPlayer(const char* url, SbWindow window,
                           SbPlayerStatusFunc player_status_func,
                           SbPlayerEncryptedMediaInitDataEncounteredCB
                               encrypted_media_init_data_encountered_cb,
                           SbPlayerErrorFunc player_error_func,
                           void* context) override;
  void SetUrlPlayerDrmSystem(SbPlayer player, SbDrmSystem drm_system) override;
  bool GetUrlPlayerOutputModeSupported(SbPlayerOutputMode output_mode) override;
  void GetUrlPlayerExtraInfo(
      SbPlayer player, SbUrlPlayerExtraInfo* out_url_player_info) override;
#endif  // SB_HAS(PLAYER_WITH_URL)
};


}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_SBPLAYER_INTERFACE_H_

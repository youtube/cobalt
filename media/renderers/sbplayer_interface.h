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

#ifndef MEDIA_RENDERERS_SBPLAYER_INTERFACE_H_
#define MEDIA_RENDERERS_SBPLAYER_INTERFACE_H_

// TODO: Move to //media/base or a Starboard specific folder.

#include "base/time/time.h"
#include "starboard/player.h"

namespace media {

class SbPlayerInterface {
 public:
  virtual ~SbPlayerInterface() {}

  virtual SbPlayer Create(
      SbWindow window,
      const SbPlayerCreationParam* creation_param,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      SbDecodeTargetGraphicsContextProvider* context_provider) = 0;
  virtual SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) = 0;
  virtual void Destroy(SbPlayer player) = 0;
  virtual void Seek(SbPlayer player,
                    base::TimeDelta seek_to_timestamp,
                    int ticket) = 0;
  virtual void WriteSamples(SbPlayer player,
                            SbMediaType sample_type,
                            const SbPlayerSampleInfo* sample_infos,
                            int number_of_sample_infos) = 0;
  virtual int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                                SbMediaType sample_type) = 0;
  virtual void WriteEndOfStream(SbPlayer player, SbMediaType stream_type) = 0;
  virtual void SetBounds(SbPlayer player,
                         int z_index,
                         int x,
                         int y,
                         int width,
                         int height) = 0;
  virtual bool SetPlaybackRate(SbPlayer player, double playback_rate) = 0;
  virtual void SetVolume(SbPlayer player, double volume) = 0;

  virtual void GetInfo(SbPlayer player, SbPlayerInfo* out_player_info) = 0;
  virtual SbDecodeTarget GetCurrentFrame(SbPlayer player) = 0;

  virtual bool GetAudioConfiguration(
      SbPlayer player,
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) = 0;
};

class DefaultSbPlayerInterface final : public SbPlayerInterface {
 public:
  DefaultSbPlayerInterface() = default;

  SbPlayer Create(
      SbWindow window,
      const SbPlayerCreationParam* creation_param,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      SbDecodeTargetGraphicsContextProvider* context_provider) override;
  SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) override;
  void Destroy(SbPlayer player) override;
  void Seek(SbPlayer player,
            base::TimeDelta seek_to_timestamp,
            int ticket) override;
  void WriteSamples(SbPlayer player,
                    SbMediaType sample_type,
                    const SbPlayerSampleInfo* sample_infos,
                    int number_of_sample_infos) override;
  int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                        SbMediaType sample_type) override;
  void WriteEndOfStream(SbPlayer player, SbMediaType stream_type) override;
  void SetBounds(SbPlayer player,
                 int z_index,
                 int x,
                 int y,
                 int width,
                 int height) override;
  bool SetPlaybackRate(SbPlayer player, double playback_rate) override;
  void SetVolume(SbPlayer player, double volume) override;
  void GetInfo(SbPlayer player, SbPlayerInfo* out_player_info) override;
  SbDecodeTarget GetCurrentFrame(SbPlayer player) override;
  bool GetAudioConfiguration(
      SbPlayer player,
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) override;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_SBPLAYER_INTERFACE_H_

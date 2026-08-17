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

#ifndef MEDIA_STARBOARD_SBPLAYER_INTERFACE_H_
#define MEDIA_STARBOARD_SBPLAYER_INTERFACE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/starboard/buildflags.h"
#include "starboard/player.h"

#if BUILDFLAG(COBALT_MEDIA_ENABLE_CVAL)
#include "cobalt/media/base/cval_stats.h"
#endif  // BUILDFLAG(COBALT_MEDIA_ENABLE_CVAL)
#if BUILDFLAG(COBALT_MEDIA_ENABLE_UMA_METRICS)
#include "cobalt/media/base/metrics_provider.h"
#endif  // BUILDFLAG(COBALT_MEDIA_ENABLE_UMA_METRICS)

#if BUILDFLAG(IS_IOS_TVOS)
#include "starboard/tvos/shared/media/url_player.h"
#endif  // BUILDFLAG(IS_IOS_TVOS)

namespace media {

class MEDIA_EXPORT SbPlayerInterface {
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

#if BUILDFLAG(IS_IOS_TVOS)
  virtual SbPlayer CreateUrlPlayer(const char* url,
                                   SbWindow window,
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
      SbPlayer player,
      SbUrlPlayerExtraInfo* out_url_player_info) = 0;
#endif  // BUILDFLAG(IS_IOS_TVOS)

  virtual bool GetAudioConfiguration(
      SbPlayer player,
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) = 0;

#if BUILDFLAG(COBALT_MEDIA_ENABLE_CVAL)
  // disabled by default, but can be enabled via h5vcc setting.
  void EnableCValStats(bool should_enable) {
    cval_stats_.Enable(should_enable);
  }
  CValStats cval_stats_;
#endif  // BUILDFLAG(COBALT_MEDIA_ENABLE_CVAL)

#if !BUILDFLAG(COBALT_MEDIA_ENABLE_UMA_METRICS)
  enum class MediaAction {
    UNKNOWN_ACTION,
    WEBMEDIAPLAYER_SEEK,
    SBPLAYER_CREATE,
    SBPLAYER_CREATE_URL_PLAYER,
    SBPLAYER_DESTROY,
    SBPLAYER_GET_PREFERRED_OUTPUT_MODE,
    SBPLAYER_SEEK,
    SBPLAYER_WRITE_END_OF_STREAM_AUDIO,
    SBPLAYER_WRITE_END_OF_STREAM_VIDEO,
    SBPLAYER_SET_BOUNDS,
    SBPLAYER_SET_PLAYBACK_RATE,
    SBPLAYER_SET_VOLUME,
    SBPLAYER_GET_INFO,
    SBPLAYER_GET_CURRENT_FRAME,
    SBPLAYER_GET_AUDIO_CONFIG,
    SBDRM_CREATE,
    SBDRM_DESTROY,
    SBDRM_GENERATE_SESSION_UPDATE_REQUEST,
    SBDRM_UPDATE_SESSION,
    SBDRM_CLOSE_SESSION,
  };
  struct MediaMetricsProvider {
    void StartTrackingAction(...) {}
    void EndTrackingAction(...) {}
  };
#endif  // !BUILDFLAG(COBALT_MEDIA_ENABLE_UMA_METRICS)
  MediaMetricsProvider media_metrics_provider_;
};

class MEDIA_EXPORT DefaultSbPlayerInterface final : public SbPlayerInterface {
 public:
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

#if BUILDFLAG(IS_IOS_TVOS)
  SbPlayer CreateUrlPlayer(const char* url,
                           SbWindow window,
                           SbPlayerStatusFunc player_status_func,
                           SbPlayerEncryptedMediaInitDataEncounteredCB
                               encrypted_media_init_data_encountered_cb,
                           SbPlayerErrorFunc player_error_func,
                           void* context) override;
  void SetUrlPlayerDrmSystem(SbPlayer player, SbDrmSystem drm_system) override;
  bool GetUrlPlayerOutputModeSupported(SbPlayerOutputMode output_mode) override;
  void GetUrlPlayerExtraInfo(
      SbPlayer player,
      SbUrlPlayerExtraInfo* out_url_player_info) override;
#endif  // BUILDFLAG(IS_IOS_TVOS)

  bool GetAudioConfiguration(
      SbPlayer player,
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) override;
};

// Sets a custom SbPlayerInterface for testing.
MEDIA_EXPORT void SetSbPlayerInterfaceForTesting(SbPlayerInterface* interface);

// Returns the current testing SbPlayerInterface instance, or nullptr if none.
MEDIA_EXPORT SbPlayerInterface* GetSbPlayerInterfaceForTesting();

// Helper class to automatically register and restore a custom SbPlayerInterface
// for testing using RAII.
//
// Lifetime and ownership:
// This object is typically instantiated as a local or member variable within a
// test scope. It does not own the passed SbPlayerInterface pointer.
//
// Threading model:
// Should be instantiated on the test thread before initiating media playback.
// The media pipeline or renderer using the mocked interface must be
// completely stopped or destroyed before this scoped object (and the underlying
// mock interface) is destroyed to avoid use-after-free on the media thread.
class MEDIA_EXPORT ScopedSbPlayerInterfaceForTesting {
 public:
  explicit ScopedSbPlayerInterfaceForTesting(SbPlayerInterface* interface)
      : previous_interface_(GetSbPlayerInterfaceForTesting()) {
    SetSbPlayerInterfaceForTesting(interface);
  }
  ~ScopedSbPlayerInterfaceForTesting() {
    SetSbPlayerInterfaceForTesting(previous_interface_);
  }
  ScopedSbPlayerInterfaceForTesting(const ScopedSbPlayerInterfaceForTesting&) =
      delete;
  ScopedSbPlayerInterfaceForTesting& operator=(
      const ScopedSbPlayerInterfaceForTesting&) = delete;

 private:
  raw_ptr<SbPlayerInterface> previous_interface_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_SBPLAYER_INTERFACE_H_

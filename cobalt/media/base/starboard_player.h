// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_STARBOARD_PLAYER_H_
#define COBALT_MEDIA_BASE_STARBOARD_PLAYER_H_

#include <map>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/decoder_buffer_cache.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/sbplayer_set_bounds_helper.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "cobalt/media/base/video_frame_provider.h"
#include "starboard/media.h"
#include "starboard/player.h"

#if SB_HAS(PLAYER_WITH_URL)
#include SB_URL_PLAYER_INCLUDE_PATH
#endif  // SB_HAS(PLAYER_WITH_URL)

namespace cobalt {
namespace media {

// TODO: Add switch to disable caching
class StarboardPlayer {
 public:
  class Host {
   public:
    virtual void OnNeedData(DemuxerStream::Type type) = 0;
    virtual void OnPlayerStatus(SbPlayerState state) = 0;
    virtual void OnPlayerError(SbPlayerError error,
                               const std::string& message) = 0;

   protected:
    ~Host() {}
  };

  // Call to get the SbDecodeTargetGraphicsContextProvider for SbPlayerCreate().
  typedef base::Callback<SbDecodeTargetGraphicsContextProvider*()>
      GetDecodeTargetGraphicsContextProviderFunc;

#if SB_HAS(PLAYER_WITH_URL)
  typedef base::Callback<void(const char*, const unsigned char*, unsigned)>
      OnEncryptedMediaInitDataEncounteredCB;
  // Create a StarboardPlayer with url-based player.
  StarboardPlayer(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const std::string& url, SbWindow window, Host* host,
      SbPlayerSetBoundsHelper* set_bounds_helper,
      bool allow_resume_after_suspend, bool prefer_decode_to_texture,
      const OnEncryptedMediaInitDataEncounteredCB&
          encrypted_media_init_data_encountered_cb,
      VideoFrameProvider* const video_frame_provider);
#endif  // SB_HAS(PLAYER_WITH_URL)
  // Create a StarboardPlayer with normal player
  StarboardPlayer(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const GetDecodeTargetGraphicsContextProviderFunc&
          get_decode_target_graphics_context_provider_func,
      const AudioDecoderConfig& audio_config,
      const VideoDecoderConfig& video_config, SbWindow window,
      SbDrmSystem drm_system, Host* host,
      SbPlayerSetBoundsHelper* set_bounds_helper,
      bool allow_resume_after_suspend, bool prefer_decode_to_texture,
      VideoFrameProvider* const video_frame_provider,
      const std::string& max_video_capabilities);

  ~StarboardPlayer();

  bool IsValid() const { return SbPlayerIsValid(player_); }

  void UpdateAudioConfig(const AudioDecoderConfig& audio_config);
  void UpdateVideoConfig(const VideoDecoderConfig& video_config);

  void WriteBuffer(DemuxerStream::Type type,
                   const scoped_refptr<DecoderBuffer>& buffer);

  void SetBounds(int z_index, const math::Rect& rect);

  void PrepareForSeek();
  void Seek(base::TimeDelta time);

  void SetVolume(float volume);
  void SetPlaybackRate(double playback_rate);
  void GetInfo(uint32* video_frames_decoded, uint32* video_frames_dropped,
               base::TimeDelta* media_time);

#if SB_HAS(PLAYER_WITH_URL)
  void GetUrlPlayerBufferedTimeRanges(base::TimeDelta* buffer_start_time,
                                      base::TimeDelta* buffer_length_time);
  void GetVideoResolution(int* frame_width, int* frame_height);
  base::TimeDelta GetDuration();
  base::TimeDelta GetStartDate();
  void SetDrmSystem(SbDrmSystem drm_system);
#endif  // SB_HAS(PLAYER_WITH_URL)

  void Suspend();
  // TODO: This is temporary for supporting background media playback.
  //       Need to be removed with media refactor.
  void Resume(SbWindow window);

  SbDecodeTarget GetCurrentSbDecodeTarget();
  SbPlayerOutputMode GetSbPlayerOutputMode();

 private:
  enum State {
    kPlaying,
    kSuspended,
    kResuming,
  };

  // This class ensures that the callbacks posted to |task_runner_| are ignored
  // automatically once StarboardPlayer is destroyed.
  class CallbackHelper : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    explicit CallbackHelper(StarboardPlayer* player);

    void ClearDecoderBufferCache();

    void OnDecoderStatus(SbPlayer player, SbMediaType type,
                         SbPlayerDecoderState state, int ticket);
    void OnPlayerStatus(SbPlayer player, SbPlayerState state, int ticket);
    void OnPlayerError(SbPlayer player, SbPlayerError error,
                       const std::string& message);
    void OnDeallocateSample(const void* sample_buffer);

    void ResetPlayer();

   private:
    base::Lock lock_;
    StarboardPlayer* player_;
  };

  static const int64 kClearDecoderCacheIntervalInMilliseconds = 1000;

  // A map from raw data pointer returned by DecoderBuffer::GetData() to the
  // DecoderBuffer and a reference count.  The reference count indicates how
  // many instances of the DecoderBuffer is currently being decoded in the
  // pipeline.
  typedef std::map<const void*, std::pair<scoped_refptr<DecoderBuffer>, int> >
      DecodingBuffers;

#if SB_HAS(PLAYER_WITH_URL)
  OnEncryptedMediaInitDataEncounteredCB
      on_encrypted_media_init_data_encountered_cb_;

  static void EncryptedMediaInitDataEncounteredCB(
      SbPlayer player, void* context, const char* init_data_type,
      const unsigned char* init_data, unsigned int init_data_length);

  void CreateUrlPlayer(const std::string& url);
#endif  // SB_HAS(PLAYER_WITH_URL)
  void CreatePlayer();

  void WriteNextBufferFromCache(DemuxerStream::Type type);
  void WriteBufferInternal(DemuxerStream::Type type,
                           const scoped_refptr<DecoderBuffer>& buffer);

  void GetInfo_Locked(uint32* video_frames_decoded,
                      uint32* video_frames_dropped,
                      base::TimeDelta* media_time);
  void UpdateBounds_Locked();

  void ClearDecoderBufferCache();

  void OnDecoderStatus(SbPlayer player, SbMediaType type,
                       SbPlayerDecoderState state, int ticket);
  void OnPlayerStatus(SbPlayer player, SbPlayerState state, int ticket);
  void OnPlayerError(SbPlayer player, SbPlayerError error,
                     const std::string& message);
  void OnDeallocateSample(const void* sample_buffer);

  static void DecoderStatusCB(SbPlayer player, void* context, SbMediaType type,
                              SbPlayerDecoderState state, int ticket);
  static void PlayerStatusCB(SbPlayer player, void* context,
                             SbPlayerState state, int ticket);
  static void PlayerErrorCB(SbPlayer player, void* context, SbPlayerError error,
                            const char* message);
  static void DeallocateSampleCB(SbPlayer player, void* context,
                                 const void* sample_buffer);

#if SB_HAS(PLAYER_WITH_URL)
  static SbPlayerOutputMode ComputeSbUrlPlayerOutputMode(
      bool prefer_decode_to_texture);
#endif  // SB_HAS(PLAYER_WITH_URL)
  // Returns the output mode that should be used for a video with the given
  // specifications.
  SbPlayerOutputMode ComputeSbPlayerOutputMode(
      bool prefer_decode_to_texture) const;

// The following variables are initialized in the ctor and never changed.
#if SB_HAS(PLAYER_WITH_URL)
  std::string url_;
#endif  // SB_HAS(PLAYER_WITH_URL)
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const GetDecodeTargetGraphicsContextProviderFunc
      get_decode_target_graphics_context_provider_func_;
  scoped_refptr<CallbackHelper> callback_helper_;
  SbWindow window_;
  SbDrmSystem drm_system_ = kSbDrmSystemInvalid;
  Host* const host_;
  // Consider merge |SbPlayerSetBoundsHelper| into CallbackHelper.
  SbPlayerSetBoundsHelper* const set_bounds_helper_;
  const bool allow_resume_after_suspend_;

  // The following variables are only changed or accessed from the
  // |task_runner_|.
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;
  SbMediaAudioSampleInfo audio_sample_info_ = {};
  SbMediaVideoSampleInfo video_sample_info_ = {};
  DecodingBuffers decoding_buffers_;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
  float volume_ = 1.0f;
  double playback_rate_ = 0.0;
  bool seek_pending_ = false;
  DecoderBufferCache decoder_buffer_cache_;

  // The following variables can be accessed from GetInfo(), which can be called
  // from any threads.  So some of their usages have to be guarded by |lock_|.
  base::Lock lock_;

  // Stores the |z_index| and |rect| parameters of the latest SetBounds() call.
  base::Optional<int> set_bounds_z_index_;
  base::Optional<math::Rect> set_bounds_rect_;
  State state_ = kPlaying;
  SbPlayer player_;
  uint32 cached_video_frames_decoded_;
  uint32 cached_video_frames_dropped_;
  base::TimeDelta preroll_timestamp_;

  // Keep track of the output mode we are supposed to output to.
  SbPlayerOutputMode output_mode_;

  VideoFrameProvider* const video_frame_provider_;

  // A string of video maximum capabilities.
  std::string max_video_capabilities_;

#if SB_HAS(PLAYER_WITH_URL)
  const bool is_url_based_;
#endif  // SB_HAS(PLAYER_WITH_URL)
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_STARBOARD_PLAYER_H_

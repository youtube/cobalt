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

#ifndef COBALT_MEDIA_BASE_SBPLAYER_BRIDGE_H_
#define COBALT_MEDIA_BASE_SBPLAYER_BRIDGE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "cobalt/media/base/cval_stats.h"
#include "cobalt/media/base/decode_target_provider.h"
#include "cobalt/media/base/decoder_buffer_cache.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "cobalt/media/base/sbplayer_set_bounds_helper.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "ui/gfx/geometry/rect.h"

namespace cobalt {
namespace media {

// TODO: Add switch to disable caching
class SbPlayerBridge {
  typedef ::media::AudioDecoderConfig AudioDecoderConfig;
  typedef ::media::DecoderBuffer DecoderBuffer;
  typedef ::media::DemuxerStream DemuxerStream;
  typedef ::media::VideoDecoderConfig VideoDecoderConfig;

 public:
  class Host {
   public:
    virtual void OnNeedData(DemuxerStream::Type type,
                            int max_number_of_buffers_to_write) = 0;
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
  // Create an SbPlayerBridge with url-based player.
  SbPlayerBridge(SbPlayerInterface* interface,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const std::string& url, SbWindow window, Host* host,
                 SbPlayerSetBoundsHelper* set_bounds_helper,
                 bool allow_resume_after_suspend,
                 SbPlayerOutputMode default_output_mode,
                 const OnEncryptedMediaInitDataEncounteredCB&
                     encrypted_media_init_data_encountered_cb,
                 DecodeTargetProvider* const decode_target_provider,
                 std::string pipeline_identifier);
#endif  // SB_HAS(PLAYER_WITH_URL)
  // Create a SbPlayerBridge with normal player
  SbPlayerBridge(SbPlayerInterface* interface,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const GetDecodeTargetGraphicsContextProviderFunc&
                     get_decode_target_graphics_context_provider_func,
                 const AudioDecoderConfig& audio_config,
                 const std::string& audio_mime_type,
                 const VideoDecoderConfig& video_config,
                 const std::string& video_mime_type, SbWindow window,
                 SbDrmSystem drm_system, Host* host,
                 SbPlayerSetBoundsHelper* set_bounds_helper,
                 bool allow_resume_after_suspend,
                 SbPlayerOutputMode default_output_mode,
                 DecodeTargetProvider* const decode_target_provider,
                 const std::string& max_video_capabilities,
                 int max_video_input_size, std::string pipeline_identifier);

  ~SbPlayerBridge();

  bool IsValid() const { return SbPlayerIsValid(player_); }

  void UpdateAudioConfig(const AudioDecoderConfig& audio_config,
                         const std::string& mime_type);
  void UpdateVideoConfig(const VideoDecoderConfig& video_config,
                         const std::string& mime_type);

  void WriteBuffers(DemuxerStream::Type type,
                    const std::vector<scoped_refptr<DecoderBuffer>>& buffers);

  void SetBounds(int z_index, const gfx::Rect& rect);

  void PrepareForSeek();
  void Seek(base::TimeDelta time);

  void SetVolume(float volume);
  void SetPlaybackRate(double playback_rate);
  void GetInfo(uint32* video_frames_decoded, uint32* video_frames_dropped,
               base::TimeDelta* media_time);
#if SB_API_VERSION >= 15
  std::vector<SbMediaAudioConfiguration> GetAudioConfigurations();
#endif  // SB_API_VERSION >= 15

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

  // These functions help the pipeline report an error message on a player
  // creation error. TryToSetPlayerCreationErrorMessage() will set
  // |player_creation_error_message_| and return true when called while
  // |is_creating_player_| is true, else it will do nothing and return false.
  bool TryToSetPlayerCreationErrorMessage(const std::string& message);
  std::string GetPlayerCreationErrorMessage() const {
    return player_creation_error_message_;
  }

  SbDecodeTarget GetCurrentSbDecodeTarget();
  SbPlayerOutputMode GetSbPlayerOutputMode();

  void RecordSetDrmSystemReadyTime(base::Time timestamp) {
    set_drm_system_ready_cb_time_ = timestamp;
  }

 private:
  enum State {
    kPlaying,
    kSuspended,
    kResuming,
  };

  // This class ensures that the callbacks posted to |task_runner_| are ignored
  // automatically once SbPlayerBridge is destroyed.
  class CallbackHelper : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    explicit CallbackHelper(SbPlayerBridge* player_bridge);

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
    SbPlayerBridge* player_bridge_;
  };

  static const int64 kClearDecoderCacheIntervalInMilliseconds = 1000;

  // A map from raw data pointer returned by DecoderBuffer::GetData() to the
  // DecoderBuffer and a reference count.  The reference count indicates how
  // many instances of the DecoderBuffer is currently being decoded in the
  // pipeline.
  typedef std::map<const void*, std::pair<scoped_refptr<DecoderBuffer>, int>>
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

  void WriteNextBuffersFromCache(DemuxerStream::Type type,
                                 int max_buffers_per_write);

  template <typename PlayerSampleInfo>
  void WriteBuffersInternal(
      DemuxerStream::Type type,
      const std::vector<scoped_refptr<DecoderBuffer>>& buffers,
      const SbMediaAudioStreamInfo* audio_stream_info,
      const SbMediaVideoStreamInfo* video_stream_info);

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
  SbPlayerOutputMode ComputeSbUrlPlayerOutputMode(
      SbPlayerOutputMode default_output_mode);
#endif  // SB_HAS(PLAYER_WITH_URL)
  // Returns the output mode that should be used for a video with the given
  // specifications.
  SbPlayerOutputMode ComputeSbPlayerOutputMode(
      SbPlayerOutputMode default_output_mode) const;

  void LogStartupLatency() const;

// The following variables are initialized in the ctor and never changed.
#if SB_HAS(PLAYER_WITH_URL)
  std::string url_;
#endif  // SB_HAS(PLAYER_WITH_URL)
  SbPlayerInterface* sbplayer_interface_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
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
  // TODO(b/268098991): Replace them with AudioStreamInfo and VideoStreamInfo
  //                    wrapper classes.
  SbMediaAudioStreamInfo audio_stream_info_ = {};
  SbMediaVideoStreamInfo video_stream_info_ = {};
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
  base::Optional<gfx::Rect> set_bounds_rect_;
  State state_ = kPlaying;
  SbPlayer player_;
  uint32 cached_video_frames_decoded_;
  uint32 cached_video_frames_dropped_;
  base::TimeDelta preroll_timestamp_;

  // Keep track of the output mode we are supposed to output to.
  SbPlayerOutputMode output_mode_;

  DecodeTargetProvider* const decode_target_provider_;

  // Keep copies of the mime type strings instead of using the ones in the
  // DemuxerStreams to ensure that the strings are always valid.
  std::string audio_mime_type_;
  std::string video_mime_type_;
  // A string of video maximum capabilities.
  std::string max_video_capabilities_;

  // Set the maximum size in bytes of an input buffer for video.
  int max_video_input_size_;

  // Keep track of errors during player creation.
  bool is_creating_player_ = false;
  std::string player_creation_error_message_;

  // Variables related to tracking player startup latencies.
  base::Time set_drm_system_ready_cb_time_{};
  base::Time player_creation_time_{};
  base::Time sb_player_state_initialized_time_{};
  base::Time sb_player_state_prerolling_time_{};
  base::Time first_audio_sample_time_{};
  base::Time first_video_sample_time_{};
  base::Time sb_player_state_presenting_time_{};

#if SB_HAS(PLAYER_WITH_URL)
  const bool is_url_based_;
#endif  // SB_HAS(PLAYER_WITH_URL)

  // Used for Gathered Sample Write.
  bool pending_audio_eos_buffer_ = false;
  bool pending_video_eos_buffer_ = false;

  CValStats* cval_stats_;
  std::string pipeline_identifier_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_SBPLAYER_BRIDGE_H_

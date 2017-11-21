// Copyright 2016 Google Inc. All Rights Reserved.
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
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/decoder_buffer_cache.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/sbplayer_set_bounds_helper.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "starboard/media.h"
#include "starboard/player.h"

namespace cobalt {
namespace media {

// TODO: Add switch to disable caching
class StarboardPlayer {
 public:
  class Host {
   public:
#if !SB_HAS(PLAYER_WITH_URL)
    virtual void OnNeedData(DemuxerStream::Type type) = 0;
#endif  // !SB_HAS(PLAYER_WITH_URL)
    virtual void OnPlayerStatus(SbPlayerState state) = 0;

   protected:
    ~Host() {}
  };

#if SB_HAS(PLAYER_WITH_URL)
  typedef base::Callback<void(const char*, const unsigned char*, unsigned)>
      OnEncryptedMediaInitDataEncounteredCB;

  StarboardPlayer(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                  const std::string& url, SbWindow window, Host* host,
                  SbPlayerSetBoundsHelper* set_bounds_helper,
                  bool prefer_decode_to_texture,
                  const OnEncryptedMediaInitDataEncounteredCB&
                      encrypted_media_init_data_encountered_cb);
#else   // SB_HAS(PLAYER_WITH_URL)
  StarboardPlayer(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                  const AudioDecoderConfig& audio_config,
                  const VideoDecoderConfig& video_config, SbWindow window,
                  SbDrmSystem drm_system, Host* host,
                  SbPlayerSetBoundsHelper* set_bounds_helper,
                  bool prefer_decode_to_texture);
#endif  // SB_HAS(PLAYER_WITH_URL)
  ~StarboardPlayer();

  bool IsValid() const { return SbPlayerIsValid(player_); }

  void UpdateVideoResolution(int frame_width, int frame_height);
  void GetVideoResolution(int* frame_width, int* frame_height);

#if !SB_HAS(PLAYER_WITH_URL)
  void WriteBuffer(DemuxerStream::Type type,
                   const scoped_refptr<DecoderBuffer>& buffer);
#endif  // !SB_HAS(PLAYER_WITH_URL)
  void SetBounds(int z_index, const gfx::Rect& rect);

  void PrepareForSeek();
  void Seek(base::TimeDelta time);

  void SetVolume(float volume);
  void SetPlaybackRate(double playback_rate);
  void GetInfo(uint32* video_frames_decoded, uint32* video_frames_dropped,
               base::TimeDelta* media_time, int* frame_width,
               int* frame_height);

#if SB_HAS(PLAYER_WITH_URL)
  base::TimeDelta GetDuration();
  void SetDrmSystem(SbDrmSystem drm_system);
#endif  // SB_HAS(PLAYER_WITH_URL)

  void Suspend();
  void Resume();

  SbDecodeTarget GetCurrentSbDecodeTarget();
  SbPlayerOutputMode GetSbPlayerOutputMode();

 private:
  enum State {
    kPlaying,
    kSuspended,
    kResuming,
  };

  // This class ensures that the callbacks posted to |message_loop_| are ignored
  // automatically once StarboardPlayer is destroyed.
  class CallbackHelper : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    explicit CallbackHelper(StarboardPlayer* player);

    void ClearDecoderBufferCache();
    void OnDecoderStatus(SbPlayer player, SbMediaType type,
                         SbPlayerDecoderState state, int ticket);
    void OnPlayerStatus(SbPlayer player, SbPlayerState state, int ticket);
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

  void CreatePlayerWithUrl(const std::string& url);
#else   // SB_HAS(PLAYER_WITH_URL)
  void CreatePlayer();
#endif  // SB_HAS(PLAYER_WITH_URL)

  void ClearDecoderBufferCache();

  void OnDecoderStatus(SbPlayer player, SbMediaType type,
                       SbPlayerDecoderState state, int ticket);
  void OnPlayerStatus(SbPlayer player, SbPlayerState state, int ticket);
  void OnDeallocateSample(const void* sample_buffer);

  static void DecoderStatusCB(SbPlayer player, void* context, SbMediaType type,
                              SbPlayerDecoderState state, int ticket);
  static void PlayerStatusCB(SbPlayer player, void* context,
                             SbPlayerState state, int ticket);
  static void DeallocateSampleCB(SbPlayer player, void* context,
                                 const void* sample_buffer);

#if SB_HAS(PLAYER_WITH_URL)
  // Returns the output mode that should be used for the URL player.
  static SbPlayerOutputMode ComputeSbPlayerOutputModeWithUrl(
      bool prefer_decode_to_texture);

  static void EncryptedMediaInitDataEncounteredCB(
      SbPlayer player, void* context, const char* init_data_type,
      const unsigned char* init_data, unsigned int init_data_length);
#else   // SB_HAS(PLAYER_WITH_URL)
  // Returns the output mode that should be used for a video with the given
  // specifications.
  static SbPlayerOutputMode ComputeSbPlayerOutputMode(
      SbMediaVideoCodec codec, SbDrmSystem drm_system,
      bool prefer_decode_to_texture);
#endif  // SB_HAS(PLAYER_WITH_URL)
  // The following variables are initialized in the ctor and never changed.
  std::string url_;
  const scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_refptr<CallbackHelper> callback_helper_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;
  const SbWindow window_;
  const SbDrmSystem drm_system_;
  Host* const host_;
  // Consider merge |SbPlayerSetBoundsHelper| into CallbackHelper.
  SbPlayerSetBoundsHelper* const set_bounds_helper_;

  // The following variables are only changed or accessed from the
  // |message_loop_|.
  int frame_width_;
  int frame_height_;
  DecodingBuffers decoding_buffers_;
  int ticket_;
  float volume_;
  double playback_rate_;
  bool paused_;
  bool seek_pending_;
  DecoderBufferCache decoder_buffer_cache_;
  // If |SetBounds| is called while we are in a suspended state, then the
  // |z_index| and |rect| that we are passed will be saved to here, and then
  // immediately set on the new player that we construct when we are resumed.
  base::optional<int> pending_set_bounds_z_index_;
  base::optional<gfx::Rect> pending_set_bounds_rect_;

  // The following variables can be accessed from GetInfo(), which can be called
  // from any threads.  So some of their usages have to be guarded by |lock_|.
  base::Lock lock_;
  State state_;
  SbPlayer player_;
  uint32 cached_video_frames_decoded_;
  uint32 cached_video_frames_dropped_;
  base::TimeDelta preroll_timestamp_;

  // Keep track of the output mode we are supposed to output to.
  SbPlayerOutputMode output_mode_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_STARBOARD_PLAYER_H_

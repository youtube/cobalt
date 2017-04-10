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
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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
class StarboardPlayer : public base::SupportsWeakPtr<StarboardPlayer> {
 public:
  class Host {
   public:
    virtual void OnNeedData(DemuxerStream::Type type) = 0;
    virtual void OnPlayerStatus(SbPlayerState state) = 0;

   protected:
    ~Host() {}
  };

  StarboardPlayer(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                  const AudioDecoderConfig& audio_config,
                  const VideoDecoderConfig& video_config, SbWindow window,
                  SbDrmSystem drm_system, Host* host,
                  SbPlayerSetBoundsHelper* set_bounds_helper,
                  bool prefer_decode_to_texture);
  ~StarboardPlayer();

  bool IsValid() const { return SbPlayerIsValid(player_); }

  void UpdateVideoResolution(int frame_width, int frame_height);

  void WriteBuffer(DemuxerStream::Type type,
                   const scoped_refptr<DecoderBuffer>& buffer);
  void SetBounds(const gfx::Rect& rect);

  void PrepareForSeek();
  void Seek(base::TimeDelta time);

  void SetVolume(float volume);
  void SetPlaybackRate(double playback_rate);
  void GetInfo(uint32* video_frames_decoded, uint32* video_frames_dropped,
               base::TimeDelta* media_time);

  void Suspend();
  void Resume();

#if SB_API_VERSION >= 4
  SbDecodeTarget GetCurrentSbDecodeTarget();
  SbPlayerOutputMode GetSbPlayerOutputMode();
#endif  // SB_API_VERSION >= 4

 private:
  enum State {
    kPlaying,
    kSuspended,
    kResuming,
  };

  static const int64 kClearDecoderCacheIntervalInMilliseconds = 1000;

  // A map from raw data pointer returned by DecoderBuffer::GetData() to the
  // DecoderBuffer and a reference count.  The reference count indicates how
  // many instances of the DecoderBuffer is currently being decoded in the
  // pipeline.
  typedef std::map<const void*, std::pair<scoped_refptr<DecoderBuffer>, int> >
      DecodingBuffers;

  void CreatePlayer();
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

#if SB_API_VERSION >= 4
  // Returns the output mode that should be used for a video with the given
  // specifications.
  static SbPlayerOutputMode ComputeSbPlayerOutputMode(
      SbMediaVideoCodec codec, SbDrmSystem drm_system,
      bool prefer_decode_to_texture);
#endif  // SB_API_VERSION >= 4

  // The following variables are initialized in the ctor and never changed.
  const scoped_refptr<base::MessageLoopProxy> message_loop_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;
  const SbWindow window_;
  const SbDrmSystem drm_system_;
  Host* const host_;
  SbPlayerSetBoundsHelper* const set_bounds_helper_;
  const base::WeakPtr<StarboardPlayer> weak_this_;

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

  // The following variables can be accessed from GetInfo(), which can be called
  // from any threads.  So some of their usages have to be guarded by |lock_|.
  base::Lock lock_;
  State state_;
  SbPlayer player_;
  uint32 cached_video_frames_decoded_;
  uint32 cached_video_frames_dropped_;
  base::TimeDelta preroll_timestamp_;

#if SB_API_VERSION >= 4
  // Keep track of the output mode we are supposed to output to.
  SbPlayerOutputMode output_mode_;
#endif  // SB_API_VERSION >= 4
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_STARBOARD_PLAYER_H_

// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_

#include <deque>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/android/shared/media_decoder.h"
#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"

namespace starboard {
namespace android {
namespace shared {

class VideoRenderer;

class VideoDecoder
    : public ::starboard::shared::starboard::player::filter::VideoDecoder,
      private MediaDecoder::Host {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;
  typedef ::starboard::shared::starboard::player::VideoFrame VideoFrame;

  VideoDecoder(SbMediaVideoCodec video_codec,
               SbDrmSystem drm_system,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider*
                   decode_target_graphics_context_provider);
  ~VideoDecoder() override;

  void Initialize(const Closure& error_cb) override;
  void WriteInputBuffer(const scoped_refptr<InputBuffer>& input_buffer)
      override;
  void WriteEndOfStream() override;
  void Reset() override;
  SbDecodeTarget GetCurrentDecodeTarget() override;

  void SetHost(VideoRenderer* host);
  bool is_valid() const { return media_decoder_ != NULL; }

 private:
  // Attempt to initialize the codec.  Returns whether initialization was
  // successful.
  bool InitializeCodec();
  void TeardownCodec();

  void ProcessOutputBuffer(MediaCodecBridge* media_codec_bridge,
                           const DequeueOutputResult& output) override;
  void RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) override;
  bool Tick(MediaCodecBridge* media_codec_bridge) override;
  void OnFlushing() override;

  // These variables will be initialized inside ctor or SetHost() and will not
  // be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  Closure error_cb_;
  VideoRenderer* host_;
  DrmSystem* drm_system_;

  SbPlayerOutputMode output_mode_;

  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;

  // If decode-to-texture is enabled, then we store the decode target texture
  // inside of this |decode_target_| member.
  SbDecodeTarget decode_target_;

  // Since GetCurrentDecodeTarget() needs to be called from an arbitrary thread
  // to obtain the current decode target (which ultimately ends up being a
  // copy of |decode_target_|), we need to safe-guard access to |decode_target_|
  // and we do so through this mutex.
  starboard::Mutex decode_target_mutex_;

  // The width and height of the latest decoded frame.
  int32_t frame_width_;
  int32_t frame_height_;

  // The last enqueued |SbMediaColorMetadata|.
  optional<SbMediaColorMetadata> previous_color_metadata_;

  // Helper value to keep track of whether we have enqueued our first input
  // buffer or not.  This is used to decide whether or not we need to
  // reinitialize upon receiving the first input buffer, since HDR metadata is
  // unforunately not provided to us at initialization time.
  bool has_written_buffer_since_reset_;

  // A queue of media codec output buffers that we have taken from the media
  // codec bridge.  It is only accessed from the |decoder_thread_|.
  std::deque<DequeueOutputResult> dequeue_output_results_;

  scoped_ptr<MediaDecoder> media_decoder_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_

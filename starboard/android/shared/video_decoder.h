// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <atomic>
#include <deque>
#include <string>

#include "starboard/android/shared/drm_system.h"
#include "starboard/android/shared/media_codec_bridge.h"
#include "starboard/android/shared/media_decoder.h"
#include "starboard/android/shared/video_frame_tracker.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/atomic.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/optional.h"
#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_render_algorithm.h"
#include "starboard/shared/starboard/player/filter/video_renderer_sink.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {
namespace android {
namespace shared {

class VideoDecoder
    : public ::starboard::shared::starboard::player::filter::VideoDecoder,
      private MediaDecoder::Host,
      private ::starboard::shared::starboard::player::JobQueue::JobOwner,
      private VideoSurfaceHolder {
 public:
  typedef ::starboard::shared::starboard::player::filter::VideoRenderAlgorithm
      VideoRenderAlgorithm;
  typedef ::starboard::shared::starboard::player::filter::VideoRendererSink
      VideoRendererSink;

  class Sink;

  static int number_of_hardware_decoders() {
    return number_of_hardware_decoders_;
  }

  VideoDecoder(SbMediaVideoCodec video_codec,
               SbDrmSystem drm_system,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider*
                   decode_target_graphics_context_provider,
               const char* max_video_capabilities,
               int tunnel_mode_audio_session_id,
               bool force_secure_pipeline_under_tunnel_mode,
               bool force_big_endian_hdr_metadata,
               std::string* error_message);
  ~VideoDecoder() override;

  scoped_refptr<VideoRendererSink> GetSink();
  scoped_ptr<VideoRenderAlgorithm> GetRenderAlgorithm();

  void Initialize(const DecoderStatusCB& decoder_status_cb,
                  const ErrorCB& error_cb) override;
  size_t GetPrerollFrameCount() const override;
  SbTime GetPrerollTimeout() const override;
  // As we hold output buffers received from MediaCodec, the max number of
  // cached frames depends on the max number of output buffers in MediaCodec,
  // which is device dependent. The media decoder may stall if we hold all
  // output buffers. But it would continue working once we release output
  // buffer.
  size_t GetMaxNumberOfCachedFrames() const override { return 12; }

  void WriteInputBuffer(
      const scoped_refptr<InputBuffer>& input_buffer) override;
  void WriteEndOfStream() override;
  void Reset() override;
  SbDecodeTarget GetCurrentDecodeTarget() override;

  void SetPlaybackRate(double playback_rate);

  void OnNewTextureAvailable();

  bool is_decoder_created() const { return media_decoder_ != NULL; }

 private:
  // Attempt to initialize the codec.  Returns whether initialization was
  // successful.
  bool InitializeCodec(std::string* error_message);
  void TeardownCodec();

  void WriteInputBufferInternal(const scoped_refptr<InputBuffer>& input_buffer);
  void ProcessOutputBuffer(MediaCodecBridge* media_codec_bridge,
                           const DequeueOutputResult& output) override;
  void OnEndOfStreamWritten(MediaCodecBridge* media_codec_bridge);
  void RefreshOutputFormat(MediaCodecBridge* media_codec_bridge) override;
  bool Tick(MediaCodecBridge* media_codec_bridge) override;
  void OnFlushing() override;

  void TryToSignalPrerollForTunnelMode();
  void OnTunnelModeFrameRendered(SbTime frame_timestamp);
  void OnTunnelModePrerollTimeout();
  void OnTunnelModeCheckForNeedMoreInput();

  void OnSurfaceDestroyed() override;
  void ReportError(SbPlayerError error, const std::string& error_message);

  static int number_of_hardware_decoders_;

  // These variables will be initialized inside ctor or Initialize() and will
  // not be changed during the life time of this class.
  const SbMediaVideoCodec video_codec_;
  DecoderStatusCB decoder_status_cb_;
  ErrorCB error_cb_;
  DrmSystem* drm_system_;
  const SbPlayerOutputMode output_mode_;
  SbDecodeTargetGraphicsContextProvider*
      decode_target_graphics_context_provider_;
  // Android doesn't officially support multi concurrent codecs. But the device
  // usually has at least one hardware decoder and Google's software decoders.
  // Google's software decoders can work concurrently. So, we use HW decoder for
  // the main player and SW decoder for sub players.
  const bool require_software_codec_;

  // Force endianness of HDR Metadata.
  const bool force_big_endian_hdr_metadata_;

  const int tunnel_mode_audio_session_id_ = -1;
  // On some platforms tunnel mode is only supported in the secure pipeline.  So
  // we create a dummy drm system to force the video playing in secure pipeline
  // to enable tunnel mode.
  scoped_ptr<DrmSystem> drm_system_to_enforce_tunnel_mode_;
  scoped_ptr<VideoFrameTracker> video_frame_tracker_;
  // Preroll in tunnel mode is handled in this class instead of in the renderer.
  atomic_bool tunnel_mode_prerolling_{true};
  atomic_bool tunnel_mode_frame_rendered_;

  // If decode-to-texture is enabled, then we store the decode target texture
  // inside of this |decode_target_| member.
  SbDecodeTarget decode_target_ = kSbDecodeTargetInvalid;

  // Since GetCurrentDecodeTarget() needs to be called from an arbitrary thread
  // to obtain the current decode target (which ultimately ends up being a
  // copy of |decode_target_|), we need to safe-guard access to |decode_target_|
  // and we do so through this mutex.
  Mutex decode_target_mutex_;

  // The width and height of the latest decoded frame.
  int32_t frame_width_ = 0;
  int32_t frame_height_ = 0;

  double playback_rate_ = 1.0;

  // The last enqueued |SbMediaColorMetadata|.
  optional<SbMediaColorMetadata> color_metadata_;

  scoped_ptr<MediaDecoder> media_decoder_;

  atomic_int32_t number_of_frames_being_decoded_;
  scoped_refptr<Sink> sink_;

  int input_buffer_written_ = 0;
  bool first_texture_received_ = false;
  bool end_of_stream_written_ = false;
  volatile SbTime first_buffer_timestamp_;
  atomic_bool has_new_texture_available_;

  // Use |owns_video_surface_| only on decoder thread, to avoid unnecessary
  // invocation of ReleaseVideoSurface(), though ReleaseVideoSurface() would
  // do nothing if not own the surface.
  bool owns_video_surface_ = false;
  Mutex surface_destroy_mutex_;
  ConditionVariable surface_condition_variable_;

  std::deque<const scoped_refptr<InputBuffer>> pending_input_buffers_;
  int video_fps_ = 0;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_DECODER_H_

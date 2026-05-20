// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/result.h"
#include "starboard/common/size.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

// GENERATED_JAVA_ENUM_PACKAGE: dev.cobalt.media
// GENERATED_JAVA_PREFIX_TO_STRIP: MEDIA_CODEC_
enum MediaCodecStatus {
  MEDIA_CODEC_OK,
  MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER,
  MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER,
  MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED,
  MEDIA_CODEC_OUTPUT_FORMAT_CHANGED,
  MEDIA_CODEC_INPUT_END_OF_STREAM,
  MEDIA_CODEC_OUTPUT_END_OF_STREAM,
  MEDIA_CODEC_NO_KEY,
  MEDIA_CODEC_INSUFFICIENT_OUTPUT_PROTECTION,
  MEDIA_CODEC_ABORT,
  MEDIA_CODEC_ERROR,

  MEDIA_CODEC_MAX = MEDIA_CODEC_ERROR,
};

struct FrameSize {
  Size display_size;
  bool has_crop_values = false;

  FrameSize() : display_size({0, 0}), has_crop_values(false) {}
  FrameSize(int width, int height, bool has_crop_values)
      : display_size({width, height}), has_crop_values(has_crop_values) {
    SB_CHECK_GE(width, 0);
    SB_CHECK_GE(height, 0);
  }
};

inline std::ostream& operator<<(std::ostream& os, const FrameSize& size) {
  return os << "{display_size=" << size.display_size
            << ", has_crop_values=" << (size.has_crop_values ? "true" : "false")
            << "}";
}

struct AudioOutputFormatResult {
  jint sample_rate;
  jint channel_count;
};

struct DequeueInputResult {
  int32_t status;
  int32_t index;
};

struct DequeueOutputResult {
  int32_t status;
  int32_t index;
  int32_t flags;
  int32_t offset;
  int64_t presentation_time_microseconds;
  int32_t num_bytes;
};

class MediaCodec {
 public:
  static constexpr int32_t kBufferFlagCodecConfig = 2;
  static constexpr int32_t kBufferFlagEndOfStream = 4;

  class Handler {
   public:
    virtual ~Handler() = default;
    virtual void OnMediaCodecInputBufferAvailable(int32_t index) = 0;
    virtual void OnMediaCodecOutputBufferAvailable(int32_t index,
                                                   int32_t flags,
                                                   int32_t offset,
                                                   int64_t presentation_time_us,
                                                   int32_t size) = 0;
    virtual void OnMediaCodecOutputFormatChanged() = 0;
    virtual void OnMediaCodecError(bool is_recoverable,
                                   bool is_transient,
                                   const std::string& error_message) = 0;
    virtual void OnMediaCodecFrameRendered(int64_t presentation_time_us) = 0;
    virtual void OnMediaCodecFirstTunnelFrameReady() = 0;
  };

  static std::unique_ptr<MediaCodec> CreateAudioMediaCodec(
      const AudioStreamInfo& audio_stream_info,
      Handler* handler,
      jobject j_media_crypto);

  static NonNullResult<std::unique_ptr<MediaCodec>> CreateVideoMediaCodec(
      SbMediaVideoCodec video_codec,
      const Size& frame_size_hint,
      int fps,
      const std::optional<Size>& max_frame_size,
      Handler* handler,
      jobject j_surface,
      jobject j_media_crypto,
      const SbMediaColorMetadata* color_metadata,
      bool enable_frame_renderer_listener,
      bool require_secured_decoder,
      bool require_software_codec,
      std::optional<int> tunnel_mode_audio_session_id,
      bool force_big_endian_hdr_metadata,
      int max_video_input_size,
      bool enable_output_checker,
      bool skip_video_frames_over_60_fps);

  static bool IsFrameRenderedCallbackEnabled();

  virtual ~MediaCodec() = default;

  virtual jni_zero::ScopedJavaLocalRef<jobject> GetInputBuffer(jint index) = 0;
  virtual void* GetInputBufferAddress(jint index, size_t* capacity) = 0;
  virtual jint QueueInputBuffer(jint index,
                                jint offset,
                                jint size,
                                jlong presentation_time_microseconds,
                                jint flags,
                                jboolean is_decode_only) = 0;
  virtual jint QueueSecureInputBuffer(jint index,
                                      jint offset,
                                      const SbDrmSampleInfo& drm_sample_info,
                                      jlong presentation_time_microseconds,
                                      jboolean is_decode_only) = 0;

  virtual jni_zero::ScopedJavaLocalRef<jobject> GetOutputBuffer(jint index) = 0;
  virtual void ReleaseOutputBuffer(jint index, jboolean render) = 0;
  virtual void ReleaseOutputBufferAtTimestamp(jint index,
                                              jlong render_timestamp_ns) = 0;

  virtual void SetPlaybackRate(double playback_rate) = 0;
  virtual bool Restart() = 0;
  virtual jint Flush() = 0;
  virtual std::optional<FrameSize> GetOutputSize() = 0;
  virtual std::optional<AudioOutputFormatResult> GetAudioOutputFormat() = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_H_

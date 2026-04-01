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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_

#include <memory>
#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/optional.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace android {
namespace shared {

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
  MEDIA_CODEC_ERROR
};

const jint BUFFER_FLAG_CODEC_CONFIG = 2;
const jint BUFFER_FLAG_END_OF_STREAM = 4;

const jint CRYPTO_MODE_UNENCRYPTED = 0;
const jint CRYPTO_MODE_AES_CTR = 1;
const jint CRYPTO_MODE_AES_CBC = 2;

struct DequeueInputResult {
  jint status;
  jint index;
};

struct DequeueOutputResult {
  jint status;
  jint index;
  jint flags;
  jint offset;
  jlong presentation_time_microseconds;
  jint num_bytes;
};

struct FrameSize {
  jint texture_width;
  jint texture_height;

  // Crop values can be set to -1 when they are not provided by the platform
  jint crop_left = -1;
  jint crop_top = -1;
  jint crop_right = -1;
  jint crop_bottom = -1;

  bool has_crop_values() const {
    return crop_left >= 0 && crop_top >= 0 && crop_right >= 0 &&
           crop_bottom >= 0;
  }

  jint display_width() const {
    if (has_crop_values()) {
      return crop_right - crop_left + 1;
    }

    return texture_width;
  }

  jint display_height() const {
    if (has_crop_values()) {
      return crop_bottom - crop_top + 1;
    }

    return texture_height;
  }

  void DCheckValid() const {
    SB_DCHECK(texture_width >= 0) << texture_width;
    SB_DCHECK(texture_height >= 0) << texture_height;

    if (crop_left >= 0 || crop_top >= 0 || crop_right >= 0 ||
        crop_bottom >= 0) {
      // If there is at least one crop value set, all of them should be set.
      SB_DCHECK(crop_left >= 0) << crop_left;
      SB_DCHECK(crop_top >= 0) << crop_top;
      SB_DCHECK(crop_right >= 0) << crop_right;
      SB_DCHECK(crop_bottom >= 0) << crop_bottom;
      SB_DCHECK(has_crop_values());
      SB_DCHECK(display_width() >= 0) << display_width();
      SB_DCHECK(display_height() >= 0) << display_height();
    }
  }
};

struct AudioOutputFormatResult {
  jint status;
  jint sample_rate;
  jint channel_count;
};

class MediaCodecBridge {
 public:
  typedef ::starboard::shared::starboard::media::AudioStreamInfo
      AudioStreamInfo;

  // The methods are called on the default Looper.  They won't get called after
  // Flush() is returned.
  class Handler {
   public:
    virtual void OnMediaCodecError(bool is_recoverable,
                                   bool is_transient,
                                   const std::string& diagnostic_info) = 0;
    virtual void OnMediaCodecCryptoError(int error_code) = 0;
    virtual void OnMediaCodecInputBufferAvailable(int buffer_index) = 0;
    virtual void OnMediaCodecOutputBufferAvailable(int buffer_index,
                                                   int flags,
                                                   int offset,
                                                   int64_t presentation_time_us,
                                                   int size) = 0;
    virtual void OnMediaCodecOutputFormatChanged() = 0;
    // This is only called on video decoder when tunnel mode is enabled.
    virtual void OnMediaCodecFrameRendered(int64_t frame_timestamp) = 0;

   protected:
    ~Handler() {}
  };

  static std::unique_ptr<MediaCodecBridge> CreateAudioMediaCodecBridge(
      const AudioStreamInfo& audio_stream_info,
      Handler* handler,
      jobject j_media_crypto);

  // `max_width` and `max_height` can be set to positive values to specify the
  // maximum resolutions the video can be adapted to.
  // When they are not set, MediaCodecBridge will set them to the maximum
  // resolutions the platform can decode.
  // Both of them have to be set at the same time (i.e. we cannot set one of
  // them without the other), which will be checked in the function.
  static std::unique_ptr<MediaCodecBridge> CreateVideoMediaCodecBridge(
      SbMediaVideoCodec video_codec,
      // `width_hint` and `height_hint` are used to create the Android video
      // format, which don't have to be directly related to the resolution of
      // the video.
      int width_hint,
      int height_hint,
      int fps,
      optional<int> max_width,
      optional<int> max_height,
      Handler* handler,
      jobject j_surface,
      jobject j_media_crypto,
      const SbMediaColorMetadata* color_metadata,
      bool require_secured_decoder,
      bool require_software_codec,
      int tunnel_mode_audio_session_id,
      bool force_big_endian_hdr_metadata,
      int max_video_input_size,
      std::string* error_message);

  ~MediaCodecBridge();

  // It is the responsibility of the client to manage the lifetime of the
  // jobject that |GetInputBuffer| returns.
  jobject GetInputBuffer(jint index);
  jint QueueInputBuffer(jint index,
                        jint offset,
                        jint size,
                        jlong presentation_time_microseconds,
                        jint flags);
  jint QueueSecureInputBuffer(jint index,
                              jint offset,
                              const SbDrmSampleInfo& drm_sample_info,
                              jlong presentation_time_microseconds);

  // It is the responsibility of the client to manage the lifetime of the
  // jobject that |GetOutputBuffer| returns.
  jobject GetOutputBuffer(jint index);
  void ReleaseOutputBuffer(jint index, jboolean render);
  void ReleaseOutputBufferAtTimestamp(jint index, jlong render_timestamp_ns);

  void SetPlaybackRate(double playback_rate);
  bool Restart();
  jint Flush();
  FrameSize GetOutputSize();
  AudioOutputFormatResult GetAudioOutputFormat();

  void OnMediaCodecError(bool is_recoverable,
                         bool is_transient,
                         const std::string& diagnostic_info);
  void OnMediaCodecCryptoError(int error_code);
  void OnMediaCodecInputBufferAvailable(int buffer_index);
  void OnMediaCodecOutputBufferAvailable(int buffer_index,
                                         int flags,
                                         int offset,
                                         int64_t presentation_time_us,
                                         int size);
  void OnMediaCodecOutputFormatChanged();
  void OnMediaCodecFrameRendered(int64_t frame_timestamp);

 private:
  // |MediaCodecBridge|s must only be created through its factory methods.
  explicit MediaCodecBridge(Handler* handler);
  void Initialize(jobject j_media_codec_bridge);

  Handler* handler_ = NULL;
  jobject j_media_codec_bridge_ = NULL;

  // Profiling and allocation tracking has identified this area to be hot,
  // and, capable of enough to cause GC times to raise high enough to impact
  // playback.  We mitigate this by reusing these output objects between calls
  // to |DequeueInputBuffer|, |DequeueOutputBuffer|, and
  // |GetOutputDimensions|.
  jobject j_reused_get_output_format_result_ = NULL;

  MediaCodecBridge(const MediaCodecBridge&) = delete;
  void operator=(const MediaCodecBridge&) = delete;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_

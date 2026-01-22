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
#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/check_op.h"
#include "starboard/common/result.h"
#include "starboard/common/size.h"
#include "starboard/shared/starboard/media/media_util.h"

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
  Size display_size;
  bool has_crop_values = false;

  FrameSize();
  FrameSize(int width, int height, bool has_crop_values);
};

std::ostream& operator<<(std::ostream& os, const FrameSize& size);

struct AudioOutputFormatResult {
  jint sample_rate;
  jint channel_count;
};

class MediaCodecBridge {
 public:
  // The methods are called on the default Looper.  They won't get called after
  // Flush() is returned.
  class Handler {
   public:
    virtual void OnMediaCodecError(bool is_recoverable,
                                   bool is_transient,
                                   const std::string& diagnostic_info) = 0;
    virtual void OnMediaCodecInputBufferAvailable(int buffer_index) = 0;
    virtual void OnMediaCodecOutputBufferAvailable(int buffer_index,
                                                   int flags,
                                                   int offset,
                                                   int64_t presentation_time_us,
                                                   int size) = 0;
    virtual void OnMediaCodecOutputFormatChanged() = 0;
    // This is called when tunnel mode is enabled or on Android 14 and newer
    // devices.
    virtual void OnMediaCodecFrameRendered(int64_t frame_timestamp) = 0;
    // This is only called on Android 12 and newer devices for tunnel mode.
    virtual void OnMediaCodecFirstTunnelFrameReady() = 0;

   protected:
    ~Handler() {}
  };

  static std::unique_ptr<MediaCodecBridge> CreateAudioMediaCodecBridge(
      const AudioStreamInfo& audio_stream_info,
      Handler* handler,
      jobject j_media_crypto);

  static NonNullResult<std::unique_ptr<MediaCodecBridge>>
  CreateVideoMediaCodecBridge(
      SbMediaVideoCodec video_codec,
      // `frame_size_hint` is used to create the Android video format, which
      // doesn't have to be directly related to the resolution of the video.
      const Size& frame_size_hint,
      int fps,
      // `max_frame_size` can be set to positive values to specify the maximum
      // resolutions the video can be adapted to.  When they are not set,
      // MediaCodecBridge will set them to the maximum resolutions the platform
      // can decode.
      const std::optional<Size>& max_frame_size,
      Handler* handler,
      jobject j_surface,
      jobject j_media_crypto,
      const SbMediaColorMetadata* color_metadata,
      bool require_secured_decoder,
      bool require_software_codec,
      int tunnel_mode_audio_session_id,
      bool force_big_endian_hdr_metadata,
      int max_video_input_size);

  ~MediaCodecBridge();

  // It is the responsibility of the client to manage the lifetime of the
  // jobject that |GetInputBuffer| returns.
  base::android::ScopedJavaLocalRef<jobject> GetInputBuffer(jint index);
  jint QueueInputBuffer(jint index,
                        jint offset,
                        jint size,
                        jlong presentation_time_microseconds,
                        jint flags,
                        jboolean is_decode_only);
  jint QueueSecureInputBuffer(jint index,
                              jint offset,
                              const SbDrmSampleInfo& drm_sample_info,
                              jlong presentation_time_microseconds,
                              jboolean is_decode_only);

  // It is the responsibility of the client to manage the lifetime of the
  // jobject that |GetOutputBuffer| returns.
  base::android::ScopedJavaLocalRef<jobject> GetOutputBuffer(jint index);
  void ReleaseOutputBuffer(jint index, jboolean render);
  void ReleaseOutputBufferAtTimestamp(jint index, jlong render_timestamp_ns);

  void SetPlaybackRate(double playback_rate);
  bool SetOutputSurface(jobject surface);
  bool Restart();
  jint Flush();
  void Stop();
  std::optional<FrameSize> GetOutputSize();
  std::optional<AudioOutputFormatResult> GetAudioOutputFormat();

  void OnMediaCodecError(
      JNIEnv* env,
      jboolean is_recoverable,
      jboolean is_transient,
      const base::android::JavaParamRef<jstring>& diagnostic_info);
  void OnMediaCodecInputBufferAvailable(JNIEnv* env, jint buffer_index);
  void OnMediaCodecOutputBufferAvailable(JNIEnv* env,
                                         jint buffer_index,
                                         jint flags,
                                         jint offset,
                                         jlong presentation_time_us,
                                         jint size);
  void OnMediaCodecOutputFormatChanged(JNIEnv* env);
  void OnMediaCodecFrameRendered(JNIEnv* env,
                                 jlong presentation_time_us,
                                 jlong render_at_system_time_ns);
  void OnMediaCodecFirstTunnelFrameReady(JNIEnv* env);

  static jboolean IsFrameRenderedCallbackEnabled();

 private:
  // |MediaCodecBridge|s must only be created through its factory methods.
  explicit MediaCodecBridge(Handler* handler);
  void Initialize(jobject j_media_codec_bridge);

  Handler* const handler_;
  base::android::ScopedJavaGlobalRef<jobject> j_media_codec_bridge_ = NULL;

  MediaCodecBridge(const MediaCodecBridge&) = delete;
  void operator=(const MediaCodecBridge&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_

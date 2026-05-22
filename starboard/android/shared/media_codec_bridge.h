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

// MediaCodecBridge is an abstract interface for Android MediaCodec
// functionality, providing a unified API for both JNI-based
// (JniMediaCodecBridge) and NDK-based (NdkMediaCodecBridge) implementations. It
// is typically owned by MediaCodecDecoder.
//
// This class is not thread-safe.
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
      bool enable_frame_renderer_listener,
      bool require_secured_decoder,
      bool require_software_codec,
      std::optional<int> tunnel_mode_audio_session_id,
      bool force_big_endian_hdr_metadata,
      int max_video_input_size,
      bool enable_output_checker,
      bool skip_video_frames_over_60_fps);

  virtual ~MediaCodecBridge();

  // It is the responsibility of the client to manage the lifetime of the
  // jobject that |GetInputBuffer| returns.
  virtual jni_zero::ScopedJavaLocalRef<jobject> GetInputBuffer(jint index) = 0;

  struct BufferAddress {
    void* address = nullptr;
    int capacity = 0;
  };
  virtual BufferAddress GetInputBufferAddress(jint index) = 0;

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

  // It is the responsibility of the client to manage the lifetime of the
  // jobject that |GetOutputBuffer| returns.
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

// JniMediaCodecBridge is an implementation of MediaCodecBridge that interacts
// with the Android MediaCodec Java API through JNI.
// It is created by the factory methods in MediaCodecBridge and is owned by
// the caller (typically a decoder).
//
// This class is not thread-safe.
class JniMediaCodecBridge : public MediaCodecBridge {
 public:
  explicit JniMediaCodecBridge(Handler* handler);
  ~JniMediaCodecBridge() override;

  void Initialize(jobject j_media_codec_bridge);

  // MediaCodecBridge implementation
  jni_zero::ScopedJavaLocalRef<jobject> GetInputBuffer(jint index) override;
  BufferAddress GetInputBufferAddress(jint index) override;
  jint QueueInputBuffer(jint index,
                        jint offset,
                        jint size,
                        jlong presentation_time_microseconds,
                        jint flags,
                        jboolean is_decode_only) override;
  jint QueueSecureInputBuffer(jint index,
                              jint offset,
                              const SbDrmSampleInfo& drm_sample_info,
                              jlong presentation_time_microseconds,
                              jboolean is_decode_only) override;

  jni_zero::ScopedJavaLocalRef<jobject> GetOutputBuffer(jint index) override;
  void ReleaseOutputBuffer(jint index, jboolean render) override;
  void ReleaseOutputBufferAtTimestamp(jint index,
                                      jlong render_timestamp_ns) override;

  void SetPlaybackRate(double playback_rate) override;
  bool Restart() override;
  jint Flush() override;
  std::optional<FrameSize> GetOutputSize() override;
  std::optional<AudioOutputFormatResult> GetAudioOutputFormat() override;

  void OnMediaCodecError(
      JNIEnv* env,
      jboolean is_recoverable,
      jboolean is_transient,
      const jni_zero::JavaParamRef<jstring>& diagnostic_info);
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

 private:
  Handler* const handler_;
  jni_zero::ScopedJavaGlobalRef<jobject> j_media_codec_bridge_ = NULL;

  JniMediaCodecBridge(const JniMediaCodecBridge&) = delete;
  void operator=(const JniMediaCodecBridge&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_

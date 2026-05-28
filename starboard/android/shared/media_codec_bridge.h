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

#include "starboard/android/shared/media_codec.h"
#include "starboard/common/result.h"
#include "starboard/common/size.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

const jint CRYPTO_MODE_UNENCRYPTED = 0;
const jint CRYPTO_MODE_AES_CTR = 1;
const jint CRYPTO_MODE_AES_CBC = 2;

class MediaCodecBridge : public MediaCodec {
 public:
  using Handler = MediaCodec::Handler;

  static std::unique_ptr<MediaCodecBridge> CreateAudioMediaCodec(
      const AudioStreamInfo& audio_stream_info,
      Handler* handler,
      jobject j_media_crypto);

  static NonNullResult<std::unique_ptr<MediaCodecBridge>> CreateVideoMediaCodec(
      SbMediaVideoCodec video_codec,
      const std::string& decoder_name,
      const char* mime,
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
      bool skip_video_frames_over_60_fps,
      bool ignore_mediacodec_callbacks_during_flushing);

  explicit MediaCodecBridge(Handler* handler);
  ~MediaCodecBridge() override;

  void Initialize(jobject j_media_codec_bridge);

  // MediaCodec implementation
  jni_zero::ScopedJavaLocalRef<jobject> GetInputBuffer(jint index) override;
  void* GetInputBufferAddress(jint index, size_t* capacity) override;
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

  // JNI callback entry points
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

  MediaCodecBridge(const MediaCodecBridge&) = delete;
  void operator=(const MediaCodecBridge&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_

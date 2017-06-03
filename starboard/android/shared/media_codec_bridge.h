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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_

#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/media_common.h"
#include "starboard/common/scoped_ptr.h"

namespace starboard {
namespace android {
namespace shared {

// These must be in sync with MediaCodecWrapper.MEDIA_CODEC_XXX constants in
// MediaCodecBridge.java.
const jint MEDIA_CODEC_OK = 0;
const jint MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER = 1;
const jint MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER = 2;
const jint MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED = 3;
const jint MEDIA_CODEC_OUTPUT_FORMAT_CHANGED = 4;
const jint MEDIA_CODEC_INPUT_END_OF_STREAM = 5;
const jint MEDIA_CODEC_OUTPUT_END_OF_STREAM = 6;
const jint MEDIA_CODEC_NO_KEY = 7;
const jint MEDIA_CODEC_ABORT = 8;
const jint MEDIA_CODEC_ERROR = 9;

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

struct SurfaceDimensions {
  jint width;
  jint height;
};

struct AudioOutputFormatResult {
  jint status;
  jint sample_rate;
  jint channel_count;
};

class MediaCodecBridge {
 public:
  static scoped_ptr<MediaCodecBridge> CreateAudioMediaCodecBridge(
      SbMediaAudioCodec audio_codec,
      const SbMediaAudioHeader& audio_header,
      jobject j_media_crypto);

  static scoped_ptr<MediaCodecBridge> CreateVideoMediaCodecBridge(
      SbMediaVideoCodec video_codec,
      int width,
      int height,
      jobject j_surface,
      jobject j_media_crypto);

  ~MediaCodecBridge();

  DequeueInputResult DequeueInputBuffer(jlong timeout_us);
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

  DequeueOutputResult DequeueOutputBuffer(jlong timeout_us);
  // It is the responsibility of the client to manage the lifetime of the
  // jobject that |GetOutputBuffer| returns.
  jobject GetOutputBuffer(jint index);
  void ReleaseOutputBuffer(jint index, jboolean render);

  jint Flush();
  SurfaceDimensions GetOutputDimensions();
  AudioOutputFormatResult GetAudioOutputFormat();

 private:
  // |MediaCodecBridge|s must only be created through its factory methods.
  explicit MediaCodecBridge(jobject j_media_codec_bridge);

  jobject j_media_codec_bridge_;

  // Profiling and allocation tracking has identified this area to be hot,
  // and, capable of enough to cause GC times to raise high enough to impact
  // playback.  We mitigate this by reusing these output objects between calls
  // to |DequeueInputBuffer|, |DequeueOutputBuffer|, and
  // |GetOutputDimensions|.
  jobject j_reused_dequeue_input_result_;
  jobject j_reused_dequeue_output_result_;
  jobject j_reused_get_output_format_result_;

  SB_DISALLOW_COPY_AND_ASSIGN(MediaCodecBridge);
};

struct OutputBufferHandle {
  jint index;
  SbTime pts_microseconds;
  jint flags;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_H_

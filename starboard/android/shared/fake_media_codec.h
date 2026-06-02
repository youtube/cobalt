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

#ifndef STARBOARD_ANDROID_SHARED_FAKE_MEDIA_CODEC_H_
#define STARBOARD_ANDROID_SHARED_FAKE_MEDIA_CODEC_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include "starboard/android/shared/media_codec.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

// FakeMediaCodec is a thread-safe simulation of Android's MediaCodec,
// used to verify decoder logic in JVM-free native unit tests.
//
// This class is thread-safe.
class FakeMediaCodec : public MediaCodec {
 public:
  explicit FakeMediaCodec(Handler* handler,
                          std::atomic<FakeMediaCodec*>* tracker);
  ~FakeMediaCodec() override;

  // MediaCodec implementation
  DataSpan GetInputBufferAddress(jint index) override;
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

  DataSpan GetOutputBufferAddress(jint index) override;
  void ReleaseOutputBuffer(jint index, jboolean render) override;
  void ReleaseOutputBufferAtTimestamp(jint index,
                                      jlong render_timestamp_ns) override;

  void SetPlaybackRate(double playback_rate) override;
  bool Restart() override;
  jint Flush() override;
  std::optional<FrameSize> GetOutputSize() override;
  std::optional<AudioOutputFormatResult> GetAudioOutputFormat() override;

  // Test control methods
  void SimulateInputBufferAvailable(int index);
  void SimulateOutputAvailable(int index,
                               int flags,
                               int offset,
                               int64_t pts,
                               int size);
  bool WaitForInputQueue(size_t num_packets, int timeout_ms);
  bool WaitForOutputReleased(size_t num_packets, int timeout_ms);

  int GetNumBuffers() const;

  struct QueuedInput {
    int index;
    int offset;
    int size;
    int64_t pts;
    int flags;
    bool is_decode_only;
  };
  std::vector<QueuedInput> GetQueuedInputs() const;

  struct ReleasedOutput {
    int index;
    bool render;
    int64_t timestamp_ns;
  };
  std::vector<ReleasedOutput> GetReleasedOutputs() const;

 private:
  Handler* const handler_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;

  std::vector<std::vector<uint8_t>> buffers_;     // Guarded by |mutex_|.
  std::vector<QueuedInput> queued_inputs_;        // Guarded by |mutex_|.
  std::vector<ReleasedOutput> released_outputs_;  // Guarded by |mutex_|.
  std::atomic<FakeMediaCodec*>* const tracker_ = nullptr;
};

// FakeMediaCodecFactory is a factory implementation used to inject
// FakeMediaCodec instances during unit tests.
//
// This class is thread-safe.
class FakeMediaCodecFactory : public MediaCodec::Factory {
 public:
  FakeMediaCodecFactory() = default;
  ~FakeMediaCodecFactory() override = default;

  std::unique_ptr<MediaCodec> CreateAudioMediaCodec(
      const AudioStreamInfo& audio_stream_info,
      MediaCodec::Handler* handler,
      jobject j_media_crypto) override;

  NonNullResult<std::unique_ptr<MediaCodec>> CreateVideoMediaCodec(
      SbMediaVideoCodec video_codec,
      const Size& frame_size_hint,
      int fps,
      const std::optional<Size>& max_frame_size,
      MediaCodec::Handler* handler,
      jobject j_surface,
      jobject j_media_crypto,
      const SbMediaColorMetadata* color_metadata,
      const MediaCodec::VideoPlatformOptions& platform_options) override;

  FakeMediaCodec* last_created_audio_codec() const {
    return last_created_audio_codec_;
  }
  FakeMediaCodec* last_created_video_codec() const {
    return last_created_video_codec_;
  }

 private:
  std::atomic<FakeMediaCodec*> last_created_audio_codec_{nullptr};
  std::atomic<FakeMediaCodec*> last_created_video_codec_{nullptr};
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_FAKE_MEDIA_CODEC_H_

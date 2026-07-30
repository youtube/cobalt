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

#include "starboard/android/shared/fake_media_codec.h"

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"

namespace starboard {
namespace {
constexpr int kNumBuffers = 8;
constexpr size_t kBufferSize = 8192;
}  // namespace

FakeMediaCodec::FakeMediaCodec(Handler* handler,
                               std::atomic<FakeMediaCodec*>* tracker)
    : handler_(handler), buffers_(kNumBuffers), tracker_(tracker) {
  SB_CHECK(handler_);
  for (int i = 0; i < kNumBuffers; ++i) {
    buffers_[i].resize(kBufferSize);
  }
}

FakeMediaCodec::~FakeMediaCodec() {
  if (tracker_) {
    FakeMediaCodec* expected = this;
    tracker_->compare_exchange_strong(expected, nullptr);
  }
}

Span<uint8_t> FakeMediaCodec::GetInputBufferAddress(jint index) {
  std::lock_guard lock(mutex_);
  if (index < 0 || index >= kNumBuffers) {
    return {};
  }
  return {buffers_[index].data(), kBufferSize};
}

jint FakeMediaCodec::QueueInputBuffer(jint index,
                                      jint offset,
                                      jint size,
                                      jlong presentation_time_microseconds,
                                      jint flags,
                                      jboolean is_decode_only) {
  std::lock_guard lock(mutex_);
  SB_CHECK_GE(index, 0);
  SB_CHECK_LT(index, kNumBuffers);

  QueuedInput input;
  input.index = index;
  input.offset = offset;
  input.size = size;
  input.pts = presentation_time_microseconds;
  input.flags = flags;
  input.is_decode_only = is_decode_only;
  queued_inputs_.push_back(input);

  cv_.notify_all();
  return 0;  // Success
}

jint FakeMediaCodec::QueueSecureInputBuffer(
    jint index,
    jint offset,
    const SbDrmSampleInfo& drm_sample_info,
    jlong presentation_time_microseconds,
    jboolean is_decode_only) {
  return -1;  // Not supported in Fake
}

Span<uint8_t> FakeMediaCodec::GetOutputBufferAddress(jint index) {
  std::lock_guard lock(mutex_);
  if (index < 0 || index >= kNumBuffers) {
    return {};
  }
  return {buffers_[index].data(), kBufferSize};
}

void FakeMediaCodec::ReleaseOutputBuffer(jint index, jboolean render) {
  std::lock_guard lock(mutex_);
  released_outputs_.push_back({index, static_cast<bool>(render), -1});
  cv_.notify_all();
}

void FakeMediaCodec::ReleaseOutputBufferAtTimestamp(jint index,
                                                    jlong render_timestamp_ns) {
  std::lock_guard lock(mutex_);
  released_outputs_.push_back({index, true, render_timestamp_ns});
  cv_.notify_all();
}

void FakeMediaCodec::SetPlaybackRate(double playback_rate) {}

bool FakeMediaCodec::Restart() {
  std::lock_guard lock(mutex_);
  SB_CHECK(queued_inputs_.empty());
  SB_CHECK(released_outputs_.empty());
  return true;
}

jint FakeMediaCodec::Flush() {
  std::lock_guard lock(mutex_);
  queued_inputs_.clear();
  released_outputs_.clear();
  return 0;
}

std::optional<FrameSize> FakeMediaCodec::GetOutputSize() {
  return FrameSize({1920, 1080}, false);
}

std::optional<AudioOutputFormatResult> FakeMediaCodec::GetAudioOutputFormat() {
  return AudioOutputFormatResult{44100, 2};
}

void FakeMediaCodec::SimulateInputBufferAvailable(int index) {
  handler_->OnMediaCodecInputBufferAvailable(index);
}

void FakeMediaCodec::SimulateOutputAvailable(int index,
                                             int flags,
                                             int offset,
                                             int64_t pts,
                                             int size) {
  handler_->OnMediaCodecOutputBufferAvailable(index, flags, offset, pts, size);
}

bool FakeMediaCodec::WaitForInputQueue(size_t num_packets, int timeout_ms) {
  std::unique_lock<std::mutex> lock(mutex_);
  return cv_.wait_for(
      lock, std::chrono::milliseconds(timeout_ms),
      [this, num_packets]() { return queued_inputs_.size() >= num_packets; });
}

bool FakeMediaCodec::WaitForOutputReleased(size_t num_packets, int timeout_ms) {
  std::unique_lock<std::mutex> lock(mutex_);
  return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                      [this, num_packets]() {
                        return released_outputs_.size() >= num_packets;
                      });
}

std::vector<FakeMediaCodec::QueuedInput> FakeMediaCodec::ConsumeQueuedInputs() {
  std::lock_guard lock(mutex_);
  return std::move(queued_inputs_);
}

std::vector<FakeMediaCodec::ReleasedOutput>
FakeMediaCodec::ConsumeReleasedOutputs() {
  std::lock_guard lock(mutex_);
  return std::move(released_outputs_);
}

// FakeMediaCodecFactory
std::unique_ptr<MediaCodec> FakeMediaCodecFactory::CreateAudioMediaCodec(
    const AudioStreamInfo& audio_stream_info,
    MediaCodec::Handler* handler,
    const jni_zero::JavaRef<jobject>& j_media_crypto) {
  auto fake =
      std::make_unique<FakeMediaCodec>(handler, &last_created_audio_codec_);
  last_created_audio_codec_ = fake.get();
  return fake;
}

NonNullResult<std::unique_ptr<MediaCodec>>
FakeMediaCodecFactory::CreateVideoMediaCodec(
    SbMediaVideoCodec video_codec,
    const Size& frame_size_hint,
    int fps,
    const std::optional<Size>& max_frame_size,
    MediaCodec::Handler* handler,
    const jni_zero::JavaRef<jobject>& j_surface,
    const jni_zero::JavaRef<jobject>& j_media_crypto,
    const SbMediaColorMetadata* color_metadata,
    const MediaCodec::VideoPlatformOptions& platform_options) {
  SB_LOG(INFO) << "[FakeMediaCodec] CreateVideoMediaCodec called";
  auto fake =
      std::make_unique<FakeMediaCodec>(handler, &last_created_video_codec_);
  last_created_video_codec_ = fake.get();
  return std::move(fake);
}

}  // namespace starboard

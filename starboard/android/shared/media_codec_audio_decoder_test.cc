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

#include "starboard/android/shared/media_codec_audio_decoder.h"

#include <unistd.h>

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "starboard/android/shared/fake_media_codec.h"
#include "starboard/android/shared/media_codec.h"
#include "starboard/common/ref_counted.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

// Values of the |enable_async_codec_creation| option.
constexpr bool kSyncCodecCreation = false;
constexpr bool kAsyncCodecCreation = true;

constexpr int kTimeoutMs = 2'000;
// Long enough for a thread that has been unblocked to finish, and for a codec
// to queue input it has been given.
constexpr int kSettleMs = 100;

const AudioStreamInfo kAudioStreamInfo = [] {
  AudioStreamInfo info;
  info.codec = kSbMediaAudioCodecAac;
  info.samples_per_second = 44100;
  info.number_of_channels = 2;
  return info;
}();

// Tests of MediaCodecAudioDecoder with |kMediaAsyncAudioCodecCreation|, where
// the codec is prepared on a helper thread while the decoder is already in
// use. The fake factory lets the tests hold the helper thread back.
class MediaCodecAudioDecoderTest : public ::testing::Test {
 protected:
  ~MediaCodecAudioDecoderTest() override {
    // A helper thread still held back would block the destructor of the
    // decoder forever.
    factory_.UnblockAudioPrepare();
    decoder_.reset();
  }

  scoped_refptr<InputBuffer> CreateInputBuffer(int64_t timestamp) {
    const int kSize = 512;
    uint8_t* buffer = new uint8_t[kSize];
    memset(buffer, 0, kSize);

    SbPlayerSampleInfo sample_info = {};
    sample_info.type = kSbMediaTypeAudio;
    sample_info.buffer = buffer;
    sample_info.buffer_size = kSize;
    sample_info.timestamp = timestamp;
    kAudioStreamInfo.ConvertTo(&sample_info.audio_sample_info.stream_info);

    auto deallocate_func = [](SbPlayer player, void* context,
                              const void* sample_buffer) {
      delete[] static_cast<const uint8_t*>(sample_buffer);
    };
    return make_scoped_refptr<InputBuffer>(deallocate_func, /*player=*/nullptr,
                                           /*context=*/nullptr, sample_info);
  }

  void CreateDecoder(bool enable_async_codec_creation) {
    auto result = MediaCodecAudioDecoder::Create(
        &job_queue_, kAudioStreamInfo, kSbDrmSystemInvalid,
        /*enable_flush_during_seek=*/true, enable_async_codec_creation,
        &factory_);
    ASSERT_TRUE(result);
    decoder_ = std::move(result.value());
    decoder_->Initialize(
        [this] { ++output_count_; },
        [this](SbPlayerError error, const std::string& message) {
          ++error_count_;
          last_error_ = error;
        });
  }

  // Creates a decoder whose helper thread is inside PrepareAudioMediaCodec()
  // and stays there until factory_.UnblockAudioPrepare() is called.
  void CreateDecoderWithPrepareBlocked() {
    factory_.BlockAudioPrepare();
    CreateDecoder(kAsyncCodecCreation);
    ASSERT_TRUE(factory_.WaitForAudioPrepareEntered(1, kTimeoutMs));
    ASSERT_EQ(codec(), nullptr);
  }

  void Decode(const std::vector<int64_t>& timestamps, int* consumed_count) {
    InputBuffers input_buffers;
    for (int64_t timestamp : timestamps) {
      input_buffers.push_back(CreateInputBuffer(timestamp));
    }
    decoder_->Decode(input_buffers, [consumed_count] { ++*consumed_count; });
  }

  // Runs the jobs of the decoder until |predicate| is true. Returns false on
  // timeout.
  bool RunJobsUntil(const std::function<bool()>& predicate) {
    const int64_t deadline_us = CurrentMonotonicTime() + kTimeoutMs * 1'000;
    while (true) {
      job_queue_.RunUntilIdle();
      if (predicate()) {
        return true;
      }
      if (CurrentMonotonicTime() > deadline_us) {
        return false;
      }
      usleep(1'000);
    }
  }

  // Runs the jobs that are due, and the ones scheduled within |kSettleMs|.
  void RunJobsAndSettle() {
    const int64_t deadline_us = CurrentMonotonicTime() + kSettleMs * 1'000;
    while (CurrentMonotonicTime() < deadline_us) {
      job_queue_.RunUntilIdle();
      usleep(1'000);
    }
    job_queue_.RunUntilIdle();
  }

  bool RunJobsUntilCodecIsCreated() {
    return RunJobsUntil([this] { return codec() != nullptr; });
  }

  // Lets the codec take |count| inputs, and returns what it was given.
  std::vector<FakeMediaCodec::QueuedInput> TakeQueuedInputs(size_t count) {
    for (size_t i = 0; i < count; ++i) {
      codec()->SimulateInputBufferAvailable(static_cast<int>(i));
    }
    EXPECT_TRUE(codec()->WaitForInputQueue(count, kTimeoutMs));
    return codec()->ConsumeQueuedInputs();
  }

  FakeMediaCodec* codec() { return factory_.last_created_audio_codec(); }

  JobQueue job_queue_;
  FakeMediaCodecFactory factory_;
  int output_count_ = 0;
  int error_count_ = 0;
  SbPlayerError last_error_ = kSbPlayerErrorCapabilityChanged;

  std::unique_ptr<MediaCodecAudioDecoder> decoder_;
};

TEST_F(MediaCodecAudioDecoderTest, CreatesCodecSynchronouslyWithoutFlag) {
  CreateDecoder(kSyncCodecCreation);

  // The codec exists as soon as the decoder does, and it was created the usual
  // way.
  ASSERT_NE(codec(), nullptr);
  EXPECT_EQ(factory_.audio_create_count(), 1);
  EXPECT_EQ(factory_.audio_prepare_count(), 0);
  EXPECT_EQ(factory_.audio_attach_count(), 0);

  int consumed_count = 0;
  Decode({1'000, 2'000}, &consumed_count);
  auto inputs = TakeQueuedInputs(2);
  ASSERT_EQ(inputs.size(), 2u);
  EXPECT_EQ(inputs[0].pts, 1'000);
  EXPECT_EQ(inputs[1].pts, 2'000);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));

  decoder_.reset();
  EXPECT_EQ(factory_.audio_prepare_count(), 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, FailsToCreateWithoutFlagWhenCodecFails) {
  factory_.set_fail_audio_create(true);

  auto result = MediaCodecAudioDecoder::Create(
      &job_queue_, kAudioStreamInfo, kSbDrmSystemInvalid,
      /*enable_flush_during_seek=*/true, kSyncCodecCreation, &factory_);

  EXPECT_FALSE(result);
  EXPECT_EQ(factory_.audio_prepare_count(), 0);
}

TEST_F(MediaCodecAudioDecoderTest, ParksInputUntilCodecIsPrepared) {
  CreateDecoderWithPrepareBlocked();

  int consumed_count = 0;
  Decode({1'000, 2'000}, &consumed_count);
  RunJobsAndSettle();

  // Nothing happens while the codec is being prepared.
  EXPECT_EQ(codec(), nullptr);
  EXPECT_EQ(consumed_count, 0);

  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  auto inputs = TakeQueuedInputs(2);
  ASSERT_EQ(inputs.size(), 2u);
  EXPECT_EQ(inputs[0].pts, 1'000);
  EXPECT_EQ(inputs[1].pts, 2'000);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  RunJobsAndSettle();
  EXPECT_EQ(consumed_count, 1);

  // The codec is the prepared one, not one created the usual way.
  EXPECT_EQ(factory_.audio_prepare_count(), 1);
  EXPECT_EQ(factory_.audio_attach_count(), 1);
  EXPECT_EQ(factory_.audio_create_count(), 0);
  EXPECT_EQ(factory_.prepared_audio_codec_released_count(), 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, WritesInputDirectlyOnceCodecIsPrepared) {
  CreateDecoderWithPrepareBlocked();
  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  int consumed_count = 0;
  Decode({1'000}, &consumed_count);

  auto inputs = TakeQueuedInputs(1);
  ASSERT_EQ(inputs.size(), 1u);
  EXPECT_EQ(inputs[0].pts, 1'000);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, ParksEndOfStreamAfterInput) {
  CreateDecoderWithPrepareBlocked();

  int consumed_count = 0;
  Decode({1'000}, &consumed_count);
  decoder_->WriteEndOfStream();
  RunJobsAndSettle();
  EXPECT_EQ(consumed_count, 0);

  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  auto inputs = TakeQueuedInputs(2);
  ASSERT_EQ(inputs.size(), 2u);
  EXPECT_EQ(inputs[0].pts, 1'000);
  EXPECT_EQ(inputs[0].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_NE(inputs[1].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  EXPECT_EQ(error_count_, 0);
}

// MediaCodecDecoder only starts its thread when input is written, so an end of
// stream written to a decoder that never got input is not queued, with or
// without kMediaAsyncAudioCodecCreation.

TEST_F(MediaCodecAudioDecoderTest, KeepsEveryDecodeCallMadeWhileParked) {
  CreateDecoderWithPrepareBlocked();

  // The audio renderer waits for the first call to be consumed before it makes
  // a second one. Should a caller not wait, nothing must be lost.
  int first_consumed_count = 0;
  int second_consumed_count = 0;
  Decode({1'000}, &first_consumed_count);
  Decode({2'000, 3'000}, &second_consumed_count);

  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  auto inputs = TakeQueuedInputs(3);
  ASSERT_EQ(inputs.size(), 3u);
  EXPECT_EQ(inputs[0].pts, 1'000);
  EXPECT_EQ(inputs[1].pts, 2'000);
  EXPECT_EQ(inputs[2].pts, 3'000);
  EXPECT_TRUE(RunJobsUntil(
      [&] { return first_consumed_count == 1 && second_consumed_count == 1; }));
  RunJobsAndSettle();
  EXPECT_EQ(first_consumed_count, 1);
  EXPECT_EQ(second_consumed_count, 1);
}

TEST_F(MediaCodecAudioDecoderTest, ResetWhilePreparingDropsParkedInput) {
  CreateDecoderWithPrepareBlocked();

  int dropped_consumed_count = 0;
  Decode({1'000}, &dropped_consumed_count);
  decoder_->WriteEndOfStream();
  decoder_->Reset();

  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());
  RunJobsAndSettle();

  // Neither the input nor the end of stream written before the reset reaches
  // the codec.
  codec()->SimulateInputBufferAvailable(0);
  EXPECT_FALSE(codec()->WaitForInputQueue(1, kSettleMs));
  EXPECT_EQ(dropped_consumed_count, 0);
  // The reset did not create another codec.
  EXPECT_EQ(factory_.audio_prepare_count(), 1);
  EXPECT_EQ(factory_.audio_create_count(), 0);

  // The decoder works after the reset.
  int consumed_count = 0;
  Decode({5'000}, &consumed_count);
  ASSERT_TRUE(codec()->WaitForInputQueue(1, kTimeoutMs));
  auto inputs = codec()->ConsumeQueuedInputs();
  ASSERT_EQ(inputs.size(), 1u);
  EXPECT_EQ(inputs[0].pts, 5'000);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  EXPECT_EQ(dropped_consumed_count, 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, InputAfterResetWhilePreparingIsParked) {
  CreateDecoderWithPrepareBlocked();

  int dropped_consumed_count = 0;
  Decode({1'000}, &dropped_consumed_count);
  decoder_->Reset();
  int consumed_count = 0;
  Decode({5'000}, &consumed_count);

  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  auto inputs = TakeQueuedInputs(1);
  ASSERT_EQ(inputs.size(), 1u);
  EXPECT_EQ(inputs[0].pts, 5'000);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  EXPECT_EQ(dropped_consumed_count, 0);
}

TEST_F(MediaCodecAudioDecoderTest, ResetAfterPreparedBeforeAttachedKeepsCodec) {
  CreateDecoderWithPrepareBlocked();

  // Let the helper thread finish and hand over the codec, without running the
  // job that attaches it.
  factory_.UnblockAudioPrepare();
  usleep(kSettleMs * 1'000);
  ASSERT_EQ(codec(), nullptr);

  // The reset must not cancel that job: the decoder would never get a codec.
  decoder_->Reset();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  int consumed_count = 0;
  Decode({5'000}, &consumed_count);
  auto inputs = TakeQueuedInputs(1);
  ASSERT_EQ(inputs.size(), 1u);
  EXPECT_EQ(inputs[0].pts, 5'000);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  EXPECT_EQ(factory_.audio_attach_count(), 1);
  EXPECT_EQ(factory_.audio_create_count(), 0);
}

TEST_F(MediaCodecAudioDecoderTest, DestroyWhilePreparingWaitsAndReleasesCodec) {
  CreateDecoderWithPrepareBlocked();

  int consumed_count = 0;
  Decode({1'000}, &consumed_count);

  // The destructor has to wait for the helper thread, which is released from
  // another thread a little later.
  const int64_t destroy_started_at_us = CurrentMonotonicTime();
  std::thread unblocker([this] {
    usleep(kSettleMs * 1'000);
    factory_.UnblockAudioPrepare();
  });
  decoder_.reset();
  const int64_t destroy_duration_us =
      CurrentMonotonicTime() - destroy_started_at_us;
  unblocker.join();

  EXPECT_GE(destroy_duration_us, kSettleMs * 1'000 / 2);
  // The codec was prepared, never attached, and released.
  EXPECT_EQ(factory_.audio_prepare_count(), 1);
  EXPECT_EQ(factory_.audio_attach_count(), 0);
  EXPECT_EQ(factory_.audio_create_count(), 0);
  EXPECT_EQ(factory_.prepared_audio_codec_destroyed_count(), 1);
  EXPECT_EQ(factory_.prepared_audio_codec_released_count(), 1);

  // Nothing of the decoder runs after it is gone.
  RunJobsAndSettle();
  EXPECT_EQ(codec(), nullptr);
  EXPECT_EQ(consumed_count, 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest,
       DestroyAfterPreparedBeforeAttachedReleasesCodec) {
  CreateDecoderWithPrepareBlocked();

  int consumed_count = 0;
  Decode({1'000}, &consumed_count);
  factory_.UnblockAudioPrepare();
  usleep(kSettleMs * 1'000);

  decoder_.reset();

  EXPECT_EQ(factory_.audio_attach_count(), 0);
  EXPECT_EQ(factory_.prepared_audio_codec_released_count(), 1);
  RunJobsAndSettle();
  EXPECT_EQ(codec(), nullptr);
  EXPECT_EQ(consumed_count, 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, DestroyWhilePreparingAfterFailedPrepare) {
  factory_.set_fail_audio_prepare(true);
  CreateDecoderWithPrepareBlocked();
  factory_.UnblockAudioPrepare();
  usleep(kSettleMs * 1'000);

  decoder_.reset();

  // No fallback and no error after the decoder is gone.
  RunJobsAndSettle();
  EXPECT_EQ(factory_.audio_create_count(), 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, FallsBackWhenPrepareFails) {
  factory_.set_fail_audio_prepare(true);
  CreateDecoderWithPrepareBlocked();

  int consumed_count = 0;
  Decode({1'000}, &consumed_count);
  decoder_->WriteEndOfStream();
  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  // The codec was created the usual way, and got the parked input.
  EXPECT_EQ(factory_.audio_create_count(), 1);
  EXPECT_EQ(factory_.audio_attach_count(), 0);
  auto inputs = TakeQueuedInputs(2);
  ASSERT_EQ(inputs.size(), 2u);
  EXPECT_EQ(inputs[0].pts, 1'000);
  EXPECT_NE(inputs[1].flags & MediaCodec::kBufferFlagEndOfStream, 0);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, FallsBackWhenAttachFails) {
  factory_.set_fail_audio_attach(true);
  CreateDecoderWithPrepareBlocked();

  int consumed_count = 0;
  Decode({1'000}, &consumed_count);
  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntilCodecIsCreated());

  // The prepared codec that could not be started was released.
  EXPECT_EQ(factory_.audio_attach_count(), 1);
  EXPECT_EQ(factory_.prepared_audio_codec_released_count(), 1);
  EXPECT_EQ(factory_.audio_create_count(), 1);
  auto inputs = TakeQueuedInputs(1);
  ASSERT_EQ(inputs.size(), 1u);
  EXPECT_EQ(inputs[0].pts, 1'000);
  EXPECT_TRUE(RunJobsUntil([&] { return consumed_count == 1; }));
  EXPECT_EQ(error_count_, 0);
}

TEST_F(MediaCodecAudioDecoderTest, ReportsErrorOnceWhenFallbackFailsToo) {
  factory_.set_fail_audio_prepare(true);
  factory_.set_fail_audio_create(true);
  CreateDecoderWithPrepareBlocked();

  int consumed_count = 0;
  Decode({1'000}, &consumed_count);
  factory_.UnblockAudioPrepare();
  ASSERT_TRUE(RunJobsUntil([this] { return error_count_ > 0; }));
  RunJobsAndSettle();

  EXPECT_EQ(error_count_, 1);
  EXPECT_EQ(last_error_, kSbPlayerErrorDecode);
  EXPECT_EQ(factory_.audio_create_count(), 1);
  EXPECT_EQ(codec(), nullptr);
  // No more input can be accepted, so the parked input is not consumed.
  EXPECT_EQ(consumed_count, 0);
}

TEST_F(MediaCodecAudioDecoderTest, ReportsErrorWithoutParkedInput) {
  // The error must not depend on input having been written.
  factory_.set_fail_audio_prepare(true);
  factory_.set_fail_audio_create(true);
  CreateDecoder(kAsyncCodecCreation);

  ASSERT_TRUE(RunJobsUntil([this] { return error_count_ > 0; }));
  RunJobsAndSettle();
  EXPECT_EQ(error_count_, 1);
}

TEST_F(MediaCodecAudioDecoderTest,
       ReportsErrorWhenFailedBeforeInitializeAndIgnoresDecode) {
  factory_.set_fail_audio_prepare(true);
  factory_.set_fail_audio_create(true);
  auto result = MediaCodecAudioDecoder::Create(
      &job_queue_, kAudioStreamInfo, kSbDrmSystemInvalid,
      /*enable_flush_during_seek=*/true, kAsyncCodecCreation, &factory_);
  ASSERT_TRUE(result);
  decoder_ = std::move(result.value());

  // Let PrepareCodec() and OnCodecPrepared() run before Initialize() sets
  // |error_cb_|.
  RunJobsAndSettle();

  decoder_->Initialize([this] { ++output_count_; },
                       [this](SbPlayerError error, const std::string& message) {
                         ++error_count_;
                         last_error_ = error;
                       });

  // Calling Decode() / WriteEndOfStream() before the scheduled ReportError()
  // job runs must not trip an assertion when |media_decoder_| is null.
  int consumed_count = 0;
  Decode({1'000}, &consumed_count);
  decoder_->WriteEndOfStream();

  ASSERT_TRUE(RunJobsUntil([this] { return error_count_ > 0; }));
  RunJobsAndSettle();
  EXPECT_EQ(error_count_, 1);
  EXPECT_EQ(last_error_, kSbPlayerErrorDecode);
  EXPECT_EQ(consumed_count, 0);
}

}  // namespace
}  // namespace starboard

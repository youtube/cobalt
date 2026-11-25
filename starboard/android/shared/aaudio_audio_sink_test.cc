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

#include "starboard/android/shared/aaudio_audio_sink.h"

#include <unistd.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kChannels = 2;
constexpr int kSampleRate = 48'000;
constexpr int kFramesPerChannel = 4'096;
constexpr int64_t kWaitTimeoutUs = 3'000'000;
// Frames are consumed when presented, i.e. at least a callback, typically the
// output latency, after they are copied, rather than right after they are
// copied in the same callback.
constexpr int64_t kMinPresentationDelayUs = 500;
// The first frames are presented well within this, unless the sink falls back
// to reporting frames as consumed when copied, as AAudio provides no
// timestamps.
constexpr int64_t kTimestampFallbackUs = 150'000;

void SetIfUnset(std::atomic<int64_t>& value, int64_t new_value) {
  int64_t expected = 0;
  value.compare_exchange_strong(expected, new_value);
}

// Records the sink callbacks. Pretends there are always frames to play.
struct FakeRenderer {
  static void UpdateSourceStatus(int* frames_in_buffer,
                                 int* offset_in_frames,
                                 bool* is_playing,
                                 bool* is_eos_reached,
                                 void* context) {
    auto* renderer = static_cast<FakeRenderer*>(context);
    EXPECT_FALSE(renderer->sink_destroyed.load());
    ++renderer->update_source_status_count;
    // The sink copies frames right after this call.
    SetIfUnset(renderer->first_update_source_status_us, CurrentMonotonicTime());
    *frames_in_buffer = kFramesPerChannel;
    *offset_in_frames = 0;
    *is_playing = true;
    *is_eos_reached = false;
  }

  static void ConsumeFrames(int frames_consumed,
                            int64_t frames_consumed_at,
                            void* context) {
    auto* renderer = static_cast<FakeRenderer*>(context);
    EXPECT_FALSE(renderer->sink_destroyed.load());
    EXPECT_GT(frames_consumed, 0);
    // No more frames than in the buffer can be consumed.
    EXPECT_LE(frames_consumed, kFramesPerChannel);
    EXPECT_LE(frames_consumed_at, CurrentMonotonicTime());
    // Before |frames_consumed|, which tests wait for.
    SetIfUnset(renderer->first_consume_frames_us, frames_consumed_at);
    renderer->frames_consumed += frames_consumed;
  }

  static void OnError(bool capability_changed,
                      const std::string& error_message,
                      void* context) {
    auto* renderer = static_cast<FakeRenderer*>(context);
    EXPECT_FALSE(renderer->sink_destroyed.load());
    ++renderer->error_count;
    renderer->last_error_capability_changed = capability_changed;
  }

  std::atomic_bool sink_destroyed = false;
  std::atomic_int update_source_status_count = 0;
  std::atomic<int64_t> frames_consumed = 0;
  // When UpdateSourceStatus() and ConsumeFrames() were first called, 0 if
  // not yet. Tests may reset them to 0 to record the next calls.
  std::atomic<int64_t> first_update_source_status_us = 0;
  std::atomic<int64_t> first_consume_frames_us = 0;
  std::atomic_int error_count = 0;
  std::atomic_bool last_error_capability_changed = false;
};

bool WaitFor(const std::function<bool()>& condition) {
  const int64_t deadline = CurrentMonotonicTime() + kWaitTimeoutUs;
  while (CurrentMonotonicTime() < deadline) {
    if (condition()) {
      return true;
    }
    usleep(5'000);
  }
  return condition();
}

class AaudioAudioSinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!AaudioAudioSink::IsSupported(kSbMediaAudioSampleTypeFloat32)) {
      GTEST_SKIP() << "AAudio isn't supported.";
    }
    AaudioAudioSink::SetOpenFailuresForTesting(0);
    AaudioAudioSink::ResetUnsupportedFormatsForTesting();
    buffer_.resize(kFramesPerChannel * kChannels, 0.0f);
    frame_buffers_[0] = buffer_.data();

    // Skip, rather than fail, if there is no working audio output.
    FakeRenderer probe_renderer;
    auto probe = CreateSink(&probe_renderer);
    const bool has_output = probe && WaitFor([&] {
                              return probe_renderer.frames_consumed.load() > 0;
                            });
    probe.reset();
    probe_renderer.sink_destroyed = true;
    AaudioAudioSink::WaitForPendingStreamOperationsForTesting();
    AaudioAudioSink::ResetUnsupportedFormatsForTesting();
    if (!has_output) {
      GTEST_SKIP() << "No working AAudio output.";
    }
  }

  void TearDown() override {
    // Don't leave stream operations running into the next test.
    AaudioAudioSink::WaitForPendingStreamOperationsForTesting();
    AaudioAudioSink::SetOpenFailuresForTesting(0);
    AaudioAudioSink::ResetUnsupportedFormatsForTesting();
  }

  std::unique_ptr<AaudioAudioSink> CreateSink(FakeRenderer* renderer) {
    return AaudioAudioSink::Create(
        kChannels, kSampleRate, kSbMediaAudioSampleTypeFloat32, frame_buffers_,
        kFramesPerChannel,
        {&FakeRenderer::UpdateSourceStatus, &FakeRenderer::ConsumeFrames,
         &FakeRenderer::OnError},
        /*is_web_audio=*/false, renderer);
  }

  std::unique_ptr<AaudioAudioSink> CreateSink() {
    return CreateSink(&renderer_);
  }

  void DestroySink(std::unique_ptr<AaudioAudioSink> sink) {
    sink.reset();
    renderer_.sink_destroyed = true;
  }

  bool WaitForFramesConsumed() {
    const int64_t frames_consumed = renderer_.frames_consumed.load();
    return WaitFor(
        [&] { return renderer_.frames_consumed.load() > frames_consumed; });
  }

  bool WaitForOpenCount(const AaudioAudioSink& sink, int count) {
    return WaitFor(
        [&] { return sink.GetStreamOpenCountForTesting() >= count; });
  }

  // Returns the rate at which frames are consumed over about 1 s, in frames
  // per second.
  int64_t MeasureConsumptionRate() {
    const int64_t start_frames = renderer_.frames_consumed.load();
    const int64_t start_us = CurrentMonotonicTime();
    usleep(1'000'000);
    const int64_t elapsed_us = CurrentMonotonicTime() - start_us;
    return (renderer_.frames_consumed.load() - start_frames) * 1'000'000 /
           elapsed_us;
  }

  // Frames are presented at the sample rate. Use loose bounds, as timestamps
  // may be stale and are extrapolated.
  void ExpectConsumptionAtSampleRate() {
    const int64_t rate = MeasureConsumptionRate();
    SB_LOG(INFO) << "Frames consumed per second: " << rate;
    EXPECT_GT(rate, kSampleRate * 7 / 10);
    EXPECT_LT(rate, kSampleRate * 13 / 10);
  }

  // Returns how long after the first frames were copied they were consumed.
  int64_t GetFirstPresentationDelayUs() {
    const int64_t first_update_us =
        renderer_.first_update_source_status_us.load();
    const int64_t first_consume_us = renderer_.first_consume_frames_us.load();
    EXPECT_NE(first_update_us, 0);
    EXPECT_NE(first_consume_us, 0);
    SB_LOG(INFO) << "First frames consumed "
                 << first_consume_us - first_update_us << " us after copied.";
    return first_consume_us - first_update_us;
  }

  FakeRenderer renderer_;
  std::vector<float> buffer_;
  void* frame_buffers_[1];
};

TEST_F(AaudioAudioSinkTest, CreateAndDestroyDontBlock) {
  const int64_t create_start_us = CurrentMonotonicTime();
  auto sink = CreateSink();
  const int64_t create_duration_us = CurrentMonotonicTime() - create_start_us;
  ASSERT_TRUE(sink);
  // Opening and starting an AAudio stream takes tens to hundreds of ms, which
  // must not happen in Create(). Use a loose bound to avoid flakiness.
  EXPECT_LT(create_duration_us, 20'000);

  ASSERT_TRUE(WaitForFramesConsumed());

  const int64_t destroy_start_us = CurrentMonotonicTime();
  DestroySink(std::move(sink));
  EXPECT_LT(CurrentMonotonicTime() - destroy_start_us, 20'000);

  // No callbacks after destruction (checked in FakeRenderer).
  const int update_count = renderer_.update_source_status_count.load();
  AaudioAudioSink::WaitForPendingStreamOperationsForTesting();
  usleep(100'000);
  EXPECT_EQ(renderer_.update_source_status_count.load(), update_count);
  EXPECT_EQ(renderer_.error_count.load(), 0);
}

TEST_F(AaudioAudioSinkTest, DestroyImmediatelyAfterCreate) {
  for (int i = 0; i < 5; ++i) {
    auto sink = CreateSink();
    ASSERT_TRUE(sink);
    sink.reset();
  }
  renderer_.sink_destroyed = true;
  // No callbacks may reach the renderer while the streams are opened (if at
  // all) and closed.
  AaudioAudioSink::WaitForPendingStreamOperationsForTesting();
  EXPECT_EQ(renderer_.error_count.load(), 0);
}

TEST_F(AaudioAudioSinkTest, FlushStopsConsumptionUntilSetStartTime) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForFramesConsumed());

  ASSERT_TRUE(sink->Flush());
  const int64_t frames_consumed = renderer_.frames_consumed.load();
  usleep(200'000);
  EXPECT_EQ(renderer_.frames_consumed.load(), frames_consumed);

  sink->SetStartTime(0);
  EXPECT_TRUE(WaitForFramesConsumed());

  DestroySink(std::move(sink));
}

// With Media.FlushAudioTrackDuringSeek, AudioRendererSinkAndroid flushes and
// reuses the sink on every seek (Reset() -> Flush(), Start() ->
// SetStartTime()), including seeks right after creation.
TEST_F(AaudioAudioSinkTest, RepeatedFlushAndSetStartTimeReusesStream) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  // Flushed before the stream is open; must stay silent once it opens.
  ASSERT_TRUE(sink->Flush());
  ASSERT_TRUE(WaitForOpenCount(*sink, 1));
  usleep(100'000);
  EXPECT_EQ(renderer_.frames_consumed.load(), 0);
  sink->SetStartTime(0);
  ASSERT_TRUE(WaitForFramesConsumed());

  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(sink->Flush());
    const int64_t frames_consumed = renderer_.frames_consumed.load();
    usleep(100'000);
    EXPECT_EQ(renderer_.frames_consumed.load(), frames_consumed);
    // The start time is unused by AaudioAudioSink.
    sink->SetStartTime(0);
    EXPECT_TRUE(WaitForFramesConsumed());
  }
  // The stream is reused, not reopened.
  EXPECT_EQ(sink->GetStreamOpenCountForTesting(), 1);
  EXPECT_EQ(renderer_.error_count.load(), 0);

  DestroySink(std::move(sink));
}

TEST_F(AaudioAudioSinkTest, DisconnectReopensStreamWithoutError) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForFramesConsumed());
  ASSERT_EQ(sink->GetStreamOpenCountForTesting(), 1);

  sink->SimulateErrorForTesting(AAUDIO_ERROR_DISCONNECTED);
  // A new stream is opened and playback continues on it.
  ASSERT_TRUE(WaitForOpenCount(*sink, 2));
  EXPECT_TRUE(WaitForFramesConsumed());
  EXPECT_EQ(renderer_.error_count.load(), 0);

  DestroySink(std::move(sink));
  EXPECT_EQ(renderer_.error_count.load(), 0);
}

TEST_F(AaudioAudioSinkTest, TooManyDisconnectsReportCapabilityChanged) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForOpenCount(*sink, 1));

  // The first 10 disconnects are recovered in the sink.
  constexpr int kMaxReopens = 10;
  for (int i = 0; i < kMaxReopens; ++i) {
    sink->SimulateErrorForTesting(AAUDIO_ERROR_DISCONNECTED);
    ASSERT_TRUE(WaitForOpenCount(*sink, i + 2));
  }
  EXPECT_EQ(renderer_.error_count.load(), 0);

  sink->SimulateErrorForTesting(AAUDIO_ERROR_DISCONNECTED);
  ASSERT_TRUE(WaitFor([&] { return renderer_.error_count.load() == 1; }));
  EXPECT_TRUE(renderer_.last_error_capability_changed.load());

  DestroySink(std::move(sink));
}

TEST_F(AaudioAudioSinkTest, ReopenFailureReportsCapabilityChanged) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForOpenCount(*sink, 1));

  // The reopen and its 3 retries (over about 1.75 s) fail.
  AaudioAudioSink::SetOpenFailuresForTesting(4);
  sink->SimulateErrorForTesting(AAUDIO_ERROR_DISCONNECTED);
  ASSERT_TRUE(WaitFor([&] { return renderer_.error_count.load() == 1; }));
  EXPECT_TRUE(renderer_.last_error_capability_changed.load());
  EXPECT_EQ(sink->GetStreamOpenCountForTesting(), 1);
  EXPECT_EQ(AaudioAudioSink::GetOpenFailuresForTesting(), 0);

  DestroySink(std::move(sink));
  // A reopen failure is likely a transient route problem, so the recreated
  // player tries AAudio again.
  FakeRenderer renderer;
  auto new_sink = CreateSink(&renderer);
  EXPECT_TRUE(new_sink);
  new_sink.reset();
  renderer.sink_destroyed = true;
}

TEST_F(AaudioAudioSinkTest, DestroyDuringReopenRetriesReportsNothing) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForOpenCount(*sink, 1));

  AaudioAudioSink::SetOpenFailuresForTesting(2);
  sink->SimulateErrorForTesting(AAUDIO_ERROR_DISCONNECTED);
  // Wait until the first reopen attempt has failed, i.e. a retry is pending.
  ASSERT_TRUE(WaitFor(
      [] { return AaudioAudioSink::GetOpenFailuresForTesting() == 1; }));
  DestroySink(std::move(sink));
  // A retry may have run before the sink was destroyed.
  const int remaining_failures = AaudioAudioSink::GetOpenFailuresForTesting();

  // Let the pending retries run (at +250 ms, +500 ms, +1 s); they must neither
  // try to open a stream for, nor report an error to, the destroyed sink.
  usleep(2'000'000);
  AaudioAudioSink::WaitForPendingStreamOperationsForTesting();
  EXPECT_EQ(AaudioAudioSink::GetOpenFailuresForTesting(), remaining_failures);
  EXPECT_EQ(renderer_.error_count.load(), 0);
}

TEST_F(AaudioAudioSinkTest, OpenFailureFallsBackForThatFormat) {
  AaudioAudioSink::SetOpenFailuresForTesting(3);
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitFor([&] { return renderer_.error_count.load() == 1; }));
  EXPECT_TRUE(renderer_.last_error_capability_changed.load());
  DestroySink(std::move(sink));

  // The recreated player falls back to another sink for the same format...
  EXPECT_FALSE(CreateSink());

  // ...until the entry expires or is cleared.
  AaudioAudioSink::ResetUnsupportedFormatsForTesting();
  FakeRenderer renderer;
  auto new_sink = CreateSink(&renderer);
  ASSERT_TRUE(new_sink);
  EXPECT_TRUE(WaitForOpenCount(*new_sink, 1));
  EXPECT_EQ(renderer.error_count.load(), 0);
  new_sink.reset();
  renderer.sink_destroyed = true;
  AaudioAudioSink::WaitForPendingStreamOperationsForTesting();
}

TEST_F(AaudioAudioSinkTest, StalledWorkerReportsCapabilityChanged) {
  // Like a hung AAudioStream_close() of another sink.
  AaudioAudioSink::StallWorkerForTesting(5'500'000);
  auto sink = CreateSink();
  ASSERT_TRUE(sink);

  // The open is queued behind the stalled operation, so the sink must report
  // an error rather than wait silently.
  const int64_t start_us = CurrentMonotonicTime();
  while (renderer_.error_count.load() == 0 &&
         CurrentMonotonicTime() - start_us < 5'000'000) {
    usleep(10'000);
  }
  EXPECT_EQ(renderer_.error_count.load(), 1);
  EXPECT_TRUE(renderer_.last_error_capability_changed.load());
  // New sinks aren't created while the worker is stalled.
  EXPECT_FALSE(CreateSink());

  // The abandoned open never runs, even after the stall ends.
  AaudioAudioSink::WaitForPendingStreamOperationsForTesting();
  usleep(200'000);
  EXPECT_EQ(sink->GetStreamOpenCountForTesting(), 0);
  EXPECT_EQ(renderer_.error_count.load(), 1);
  DestroySink(std::move(sink));
}

TEST_F(AaudioAudioSinkTest, NonDisconnectErrorIsReported) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForFramesConsumed());

  sink->SimulateErrorForTesting(AAUDIO_ERROR_INTERNAL);
  EXPECT_EQ(renderer_.error_count.load(), 1);
  EXPECT_FALSE(renderer_.last_error_capability_changed.load());

  DestroySink(std::move(sink));
}

TEST_F(AaudioAudioSinkTest, FramesAreConsumedWhenPresented) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForFramesConsumed());
  // Without AAudio timestamps, the sink falls back to reporting the frames as
  // consumed when copied after a timeout, so this also holds.
  EXPECT_GE(GetFirstPresentationDelayUs(), kMinPresentationDelayUs);

  ExpectConsumptionAtSampleRate();
  // Media time doesn't lead real time (allowing 50 ms for timestamp jitter).
  const int64_t elapsed_us =
      CurrentMonotonicTime() - renderer_.first_update_source_status_us.load();
  EXPECT_LE(renderer_.frames_consumed.load(),
            (elapsed_us + 50'000) * kSampleRate / 1'000'000);
  EXPECT_EQ(renderer_.error_count.load(), 0);

  DestroySink(std::move(sink));
}

TEST_F(AaudioAudioSinkTest, FlushDropsFramesInFlight) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForFramesConsumed());
  // Without AAudio timestamps, the sink reports frames as consumed when copied
  // after a timeout (200 ms), also after Flush().
  const bool has_timestamps =
      GetFirstPresentationDelayUs() < kTimestampFallbackUs;
  usleep(100'000);

  // Frames copied but not presented yet are pre-seek frames, so they are never
  // reported as consumed.
  ASSERT_TRUE(sink->Flush());
  const int64_t frames_consumed = renderer_.frames_consumed.load();
  usleep(200'000);
  EXPECT_EQ(renderer_.frames_consumed.load(), frames_consumed);

  // No callbacks reach the renderer until SetStartTime().
  renderer_.first_update_source_status_us = 0;
  renderer_.first_consume_frames_us = 0;
  sink->SetStartTime(0);
  ASSERT_TRUE(WaitForFramesConsumed());
  if (has_timestamps) {
    EXPECT_GE(GetFirstPresentationDelayUs(), kMinPresentationDelayUs);
  }
  ExpectConsumptionAtSampleRate();
  EXPECT_EQ(sink->GetStreamOpenCountForTesting(), 1);
  EXPECT_EQ(renderer_.error_count.load(), 0);

  DestroySink(std::move(sink));
}

TEST_F(AaudioAudioSinkTest, ReopenedStreamContinuesConsumption) {
  auto sink = CreateSink();
  ASSERT_TRUE(sink);
  ASSERT_TRUE(WaitForFramesConsumed());
  usleep(100'000);

  sink->SimulateErrorForTesting(AAUDIO_ERROR_DISCONNECTED);
  ASSERT_TRUE(WaitForOpenCount(*sink, 2));
  ASSERT_TRUE(WaitForFramesConsumed());
  // Frames presented by the new stream are consumed at the sample rate.
  usleep(200'000);
  ExpectConsumptionAtSampleRate();
  EXPECT_EQ(renderer_.error_count.load(), 0);

  DestroySink(std::move(sink));
}

}  // namespace
}  // namespace starboard

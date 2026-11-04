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

#include "starboard/common/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

constexpr int kChannels = 2;
constexpr int kSampleRate = 48'000;
constexpr int kFramesPerChannel = 4'096;
constexpr int64_t kWaitTimeoutUs = 3'000'000;

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

}  // namespace
}  // namespace starboard

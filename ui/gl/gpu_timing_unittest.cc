// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_timing.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_timing_fake.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;

class GPUTimingTest : public testing::Test {
 public:
  void SetUp() override { SetupGLContext(); }

  void TearDown() override {
    context_ = nullptr;
    surface_ = nullptr;
    MockGLInterface::SetGLInterface(nullptr);
    GLSurfaceTestSupport::ShutdownGL(display_);
    cpu_time_bounded_ = false;
    gl_.reset();
    gpu_timing_fake_queries_.Reset();
  }

  scoped_refptr<GPUTimingClient> CreateGPUTimingClient() {
    EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, _))
        .WillOnce(SetArgPointee<1>(0))
        .RetiresOnSaturation();
    scoped_refptr<GPUTimingClient> client = context_->CreateGPUTimingClient();
    if (!cpu_time_bounded_) {
      client->SetCpuTimeForTesting(
          base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));
      cpu_time_bounded_ = true;
    }
    return client;
  }

 protected:
  void SetupGLContext() {
    SetGLGetProcAddressProc(MockGLInterface::GetGLProcAddress);
    display_ = GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<::testing::StrictMock<MockGLInterface>>();
    MockGLInterface::SetGLInterface(gl_.get());

    context_ = new GLContextStub;
    context_->SetExtensionsString("GL_EXT_disjoint_timer_query");
    context_->SetGLVersionString("OpenGL ES 3.0");
    surface_ = new GLSurfaceStub;
    context_->MakeCurrent(surface_.get());
    gpu_timing_fake_queries_.Reset();
  }

  bool cpu_time_bounded_ = false;
  std::unique_ptr<::testing::StrictMock<MockGLInterface>> gl_;
  scoped_refptr<GLContextStub> context_;
  scoped_refptr<GLSurfaceStub> surface_;
  GPUTimingFake gpu_timing_fake_queries_;
  raw_ptr<GLDisplay> display_ = nullptr;
};

TEST_F(GPUTimingTest, FakeTimerTest) {
  scoped_refptr<GPUTimingClient> gpu_timing_client = CreateGPUTimingClient();

  // Tests that we can properly set fake cpu times.
  gpu_timing_fake_queries_.SetCurrentCPUTime(123);
  EXPECT_EQ(123, gpu_timing_client->GetCurrentCPUTime());

  base::RepeatingCallback<int64_t(void)> empty;
  gpu_timing_client->SetCpuTimeForTesting(empty);
  EXPECT_NE(123, gpu_timing_client->GetCurrentCPUTime());
}

TEST_F(GPUTimingTest, ForceTimeElapsedQuery) {
  // Test that forcing time elapsed query affects all clients.
  scoped_refptr<GPUTimingClient> client1 = CreateGPUTimingClient();
  EXPECT_FALSE(client1->IsForceTimeElapsedQuery());

  scoped_refptr<GPUTimingClient> client_force = CreateGPUTimingClient();
  EXPECT_FALSE(client1->IsForceTimeElapsedQuery());
  client_force->ForceTimeElapsedQuery();
  EXPECT_TRUE(client1->IsForceTimeElapsedQuery());

  EXPECT_TRUE(client1->IsForceTimeElapsedQuery());

  scoped_refptr<GPUTimingClient> client2 = CreateGPUTimingClient();
  EXPECT_TRUE(client2->IsForceTimeElapsedQuery());
}

TEST_F(GPUTimingTest, QueryTimeStampTest) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();
  std::unique_ptr<GPUTimer> gpu_timer = client->CreateGPUTimer(false);

  const int64_t begin_cpu_time = 1230;
  const int64_t begin_gl_time = 10 * base::Time::kNanosecondsPerMicrosecond;
  const int64_t cpu_gl_offset =
      begin_gl_time / base::Time::kNanosecondsPerMicrosecond - begin_cpu_time;
  gpu_timing_fake_queries_.SetCPUGLOffset(cpu_gl_offset);
  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time);
  gpu_timing_fake_queries_.ExpectGPUTimeStampQuery(*gl_, false);

  gpu_timer->QueryTimeStamp();

  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time - 1);
  EXPECT_FALSE(gpu_timer->IsAvailable());

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time);
  EXPECT_TRUE(gpu_timer->IsAvailable());

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time + 1);
  EXPECT_TRUE(gpu_timer->IsAvailable());

  EXPECT_EQ(0, gpu_timer->GetDeltaElapsed());

  int64_t start, end;
  gpu_timer->GetStartEndTimestamps(&start, &end);
  EXPECT_EQ(begin_cpu_time, start);
  EXPECT_EQ(begin_cpu_time, end);
}

TEST_F(GPUTimingTest, QueryTimestampUsingElapsedTest) {
  // Test timestamp queries on platforms with GL_EXT_disjoint_timer_query but
  // still lack support for timestamp queries
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();
  std::unique_ptr<GPUTimer> gpu_timer = client->CreateGPUTimer(false);

  const int64_t begin_cpu_time = 123;
  const int64_t begin_gl_time = 10 * base::Time::kNanosecondsPerMicrosecond;
  const int64_t cpu_gl_offset = begin_gl_time - begin_cpu_time;
  gpu_timing_fake_queries_.SetCPUGLOffset(cpu_gl_offset);
  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time);

  gpu_timing_fake_queries_.ExpectGPUTimeStampQuery(*gl_, true);

  // Custom mock override to ensure the timestamp bits are 0
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .Times(Exactly(1))
      .WillRepeatedly(DoAll(SetArgPointee<2>(0), Return()));

  gpu_timer->QueryTimeStamp();

  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time - 1);
  EXPECT_FALSE(gpu_timer->IsAvailable());

  gpu_timing_fake_queries_.SetCurrentCPUTime(begin_cpu_time + 1);
  EXPECT_TRUE(gpu_timer->IsAvailable());
  EXPECT_EQ(0, gpu_timer->GetDeltaElapsed());

  int64_t start, end;
  gpu_timer->GetStartEndTimestamps(&start, &end);
  // Force time elapsed won't be set until a query is actually attempted
  ASSERT_TRUE(client->IsForceTimeElapsedQuery());
  EXPECT_EQ(begin_cpu_time, start);
  EXPECT_EQ(begin_cpu_time, end);
}

#if BUILDFLAG(IS_COBALT)
// When video playback initializes on a GPU driver that reports 0 counter bits
// for GL_TIMESTAMP_EXT, GPUTiming immediately reports timestamp queries as
// imprecise and forces elapsed-time query emulation so presentation does not
// use uncalibrated zero-bit counters.
TEST_F(GPUTimingTest, AreTimestampQueriesPreciseZeroCounterBits) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillOnce(DoAll(SetArgPointee<2>(0), Return()));

  EXPECT_FALSE(client->AreTimestampQueriesPrecise());
  EXPECT_TRUE(client->IsForceTimeElapsedQuery());
}

// During multi-day video playback on a conformant 64-bit GPU driver with
// non-power-of-two nanosecond query resolution, GPUTiming remains marked as
// precise even at multi-day uptime (72h).
TEST_F(GPUTimingTest, AreTimestampQueriesPreciseHighUptimeFull64Bit) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();

  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .Times(AtLeast(8))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_queries_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .Times(AtLeast(8))
      .WillRepeatedly(Invoke(&gpu_timing_fake_queries_,
                             &GPUTimingFake::FakeGLDeleteQueries));

  int64_t current_ns = 72LL * 3600 * base::Time::kNanosecondsPerSecond + 373;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT)).Times(AtLeast(8));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(
          Invoke([&](GLenum, GLint64* data) { *data = current_ns; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke([&](GLuint, GLenum, GLuint64* params) {
        *params = static_cast<GLuint64>(current_ns);
      }));

  EXPECT_TRUE(client->AreTimestampQueriesPrecise());
  for (int i = 0; i < 8; ++i) {
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    std::unique_ptr<GPUTimer> timer = client->CreateGPUTimer(false);
    timer->QueryTimeStamp();
    current_ns += 16'666'667;  // ~60 Hz with non-power-of-two low bits.
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    ASSERT_TRUE(timer->IsAvailable());
    EXPECT_TRUE(client->AreTimestampQueriesPrecise());
    timer->Destroy(true);
  }
}

// When video playback or UI rendering runs on GPU drivers that truncate
// GL_TIMESTAMP_EXT query results through binary32 float at high uptime
// (e.g. Mali at 31 hours), GPUTiming detects the coarse step size after 6
// samples and marks timestamp queries as imprecise across all GPUTimingClients
// sharing the GLContext.
TEST_F(GPUTimingTest, AreTimestampQueriesPreciseDetectsFloat32Truncation) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();

  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .Times(AtLeast(6))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_queries_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .Times(AtLeast(6))
      .WillRepeatedly(Invoke(&gpu_timing_fake_queries_,
                             &GPUTimingFake::FakeGLDeleteQueries));

  // Simulate Mali's float32 truncation on GL_QUERY_RESULT at 31 hours uptime
  // while glGetInteger64v(GL_TIMESTAMP_EXT) returns full 64-bit nanoseconds.
  int64_t current_ns = 31LL * 3600 * base::Time::kNanosecondsPerSecond + 12345;
  uint64_t query_ns = 0;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT))
      .Times(AtLeast(6))
      .WillRepeatedly(Invoke([&](GLuint, GLenum) {
        query_ns = static_cast<uint64_t>(static_cast<float>(current_ns));
      }));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(
          Invoke([&](GLenum, GLint64* data) { *data = current_ns; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke(
          [&](GLuint, GLenum, GLuint64* params) { *params = query_ns; }));

  EXPECT_TRUE(client->AreTimestampQueriesPrecise());
  for (int i = 0; i < 6; ++i) {
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    std::unique_ptr<GPUTimer> timer = client->CreateGPUTimer(false);
    timer->QueryTimeStamp();
    current_ns += 8'000'000;
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    ASSERT_TRUE(timer->IsAvailable());
    if (i < 5) {
      EXPECT_TRUE(client->AreTimestampQueriesPrecise());
    } else {
      EXPECT_FALSE(client->AreTimestampQueriesPrecise());
    }
    timer->Destroy(true);
  }

  // Subsequent GPUTimingClients sharing the GLContext inherit the fallback.
  scoped_refptr<GPUTimingClient> client2 = CreateGPUTimingClient();
  EXPECT_FALSE(client2->AreTimestampQueriesPrecise());
}

// At video playback startup on drivers where glGetInteger64v(GL_TIMESTAMP_EXT)
// and query results use mismatched timebases (e.g. 24 MHz ticks vs
// nanoseconds), GPUTiming detects the multi-second calibration skew after 2
// samples and disables timestamp queries for presentation.
TEST_F(GPUTimingTest, AreTimestampQueriesPreciseDetectsCalibrationMismatch) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();

  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .Times(AtLeast(2))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_queries_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .Times(AtLeast(2))
      .WillRepeatedly(Invoke(&gpu_timing_fake_queries_,
                             &GPUTimingFake::FakeGLDeleteQueries));

  // Simulate unpatched Mali where glGetInteger64v(GL_TIMESTAMP_EXT) returns raw
  // 24 MHz ticks while glGetQueryObjectui64v(GL_QUERY_RESULT) is in ns.
  int64_t current_ns = 3600LL * base::Time::kNanosecondsPerSecond + 777;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT)).Times(AtLeast(2));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(Invoke(
          [&](GLenum, GLint64* data) { *data = (current_ns * 24) / 1000; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke([&](GLuint, GLenum, GLuint64* params) {
        *params = static_cast<GLuint64>(current_ns);
      }));

  EXPECT_TRUE(client->AreTimestampQueriesPrecise());
  for (int i = 0; i < 2; ++i) {
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    std::unique_ptr<GPUTimer> timer = client->CreateGPUTimer(false);
    timer->QueryTimeStamp();
    current_ns += 16'667'000;
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    ASSERT_TRUE(timer->IsAvailable());
    if (i == 0) {
      EXPECT_TRUE(client->AreTimestampQueriesPrecise());
    } else {
      EXPECT_FALSE(client->AreTimestampQueriesPrecise());
    }
    timer->Destroy(true);
  }
}

// During continuous video playback across driver uptime transitions,
// float32-quantized timestamps below the 8 us step threshold (< 68.7 s uptime,
// ULP = 4.096 us) remain accepted as precise, while steps >= 8 us (>= 68.7 s
// uptime, ULP = 8.192 us) trigger fallback after 6 samples.
TEST_F(GPUTimingTest, AreTimestampQueriesPreciseStepThresholdBoundary) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();

  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .Times(AtLeast(12))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_queries_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .Times(AtLeast(12))
      .WillRepeatedly(Invoke(&gpu_timing_fake_queries_,
                             &GPUTimingFake::FakeGLDeleteQueries));

  int64_t current_ns = 0;
  uint64_t query_ns = 0;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT))
      .Times(AtLeast(12))
      .WillRepeatedly(Invoke([&](GLuint, GLenum) {
        query_ns = static_cast<uint64_t>(static_cast<float>(current_ns));
      }));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(
          Invoke([&](GLenum, GLint64* data) { *data = current_ns; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke(
          [&](GLuint, GLenum, GLuint64* params) { *params = query_ns; }));

  // Below 2^36 ns (~68.7 s), binary32 ULP is 2^12 ns = 4.096 us (< 8 us).
  current_ns = (int64_t{1} << 35) + (int64_t{3} << 12);
  for (int i = 0; i < 6; ++i) {
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    std::unique_ptr<GPUTimer> timer = client->CreateGPUTimer(false);
    timer->QueryTimeStamp();
    current_ns += (int64_t{4069} << 12);  // ~16.67 ms, odd multiple of 2^12 ns.
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    ASSERT_TRUE(timer->IsAvailable());
    EXPECT_TRUE(client->AreTimestampQueriesPrecise());
    timer->Destroy(true);
  }

  // At >= 2^36 ns (~68.7 s), binary32 ULP reaches 2^13 ns = 8.192 us (>= 8 us).
  current_ns = (int64_t{1} << 36) + (int64_t{3} << 13);
  for (int i = 0; i < 6; ++i) {
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    std::unique_ptr<GPUTimer> timer = client->CreateGPUTimer(false);
    timer->QueryTimeStamp();
    current_ns += (int64_t{2035} << 13);  // ~16.67 ms, odd multiple of 2^13 ns.
    gpu_timing_fake_queries_.SetCurrentCPUTime(current_ns / 1000);
    ASSERT_TRUE(timer->IsAvailable());
    if (i < 5) {
      EXPECT_TRUE(client->AreTimestampQueriesPrecise());
    } else {
      EXPECT_FALSE(client->AreTimestampQueriesPrecise());
    }
    timer->Destroy(true);
  }
}

// When a GPU driver hangs or stalls its GL_TIMESTAMP_EXT counter mid-playback,
// the frozen counter is detected after 6 stalled intervals (>= 4 ms apart, 7
// samples total), even when mid-frame queries arrive 2-4 ms apart in between.
TEST_F(GPUTimingTest, AreTimestampQueriesPreciseDetectsStalledClock) {
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();

  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  gpu_timing_fake_queries_.ExpectDisjointCalls(*gl_);
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .Times(AtLeast(7))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_queries_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .Times(AtLeast(7))
      .WillRepeatedly(Invoke(&gpu_timing_fake_queries_,
                             &GPUTimingFake::FakeGLDeleteQueries));

  const GLuint64 frozen_ns = 1'000'000'123ULL;
  int64_t cpu_us = 1'000'000;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT)).Times(AtLeast(7));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(frozen_ns), Return()));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(frozen_ns), Return()));

  // Sample 0 establishes baseline; samples 1..6 observe 6 consecutive stalled
  // readings across >= 4ms CPU intervals, with an extra 2.5ms mid-frame query
  // that must not reset the stalled-clock streak.
  for (int i = 0; i < 7; ++i) {
    gpu_timing_fake_queries_.SetCurrentCPUTime(cpu_us);
    std::unique_ptr<GPUTimer> timer = client->CreateGPUTimer(false);
    timer->QueryTimeStamp();
    if (i == 2) {
      cpu_us += 2'500;
      gpu_timing_fake_queries_.SetCurrentCPUTime(cpu_us);
      std::unique_ptr<GPUTimer> mid_frame_timer = client->CreateGPUTimer(false);
      mid_frame_timer->QueryTimeStamp();
      ASSERT_TRUE(mid_frame_timer->IsAvailable());
      mid_frame_timer->Destroy(true);
    }
    cpu_us += 16'000;
    gpu_timing_fake_queries_.SetCurrentCPUTime(cpu_us);
    ASSERT_TRUE(timer->IsAvailable());
    if (i < 6) {
      EXPECT_TRUE(client->AreTimestampQueriesPrecise());
    } else {
      EXPECT_FALSE(client->AreTimestampQueriesPrecise());
    }
    timer->Destroy(true);
  }
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace gl

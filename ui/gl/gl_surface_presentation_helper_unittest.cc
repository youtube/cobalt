// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_presentation_helper.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_COBALT) && !BUILDFLAG(IS_ANDROID)

#include <algorithm>
#include <atomic>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gpu_timing.h"
#include "ui/gl/gpu_timing_fake.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gl {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;

// GLContextStub subclass exposing a test GLDisplayEGL (and optional null
// GPUTimingClient simulation) so GLFence::IsSupported() and EGL sync creation
// follow the EGL path under test.
//
// Lifetime and ownership: Ref-counted via GLContext; owned by the test fixture
// and dropped in TearDown() before GLDisplayEGL shutdown.
// Threading model: Single-threaded; used exclusively on the test's main/GPU
// task runner.
class TestGLContextWithEGLDisplay : public GLContextStub {
 public:
  explicit TestGLContextWithEGLDisplay(GLDisplayEGL* display_egl)
      : display_egl_(display_egl) {}

  void set_return_null_gpu_timing_client(bool value) {
    return_null_gpu_timing_client_ = value;
  }

  void set_fail_make_current(bool value) { fail_make_current_ = value; }

  int make_current_calls() const { return make_current_calls_; }

  bool MakeCurrentImpl(GLSurface* surface) override {
    ++make_current_calls_;
    if (fail_make_current_) {
      return false;
    }
    return GLContextStub::MakeCurrentImpl(surface);
  }

  GLDisplayEGL* GetGLDisplayEGL() override { return display_egl_; }
  scoped_refptr<GPUTimingClient> CreateGPUTimingClient() override {
    if (return_null_gpu_timing_client_) {
      return nullptr;
    }
    return GLContextStub::CreateGPUTimingClient();
  }

 private:
  ~TestGLContextWithEGLDisplay() override = default;
  raw_ptr<GLDisplayEGL> display_egl_;
  bool return_null_gpu_timing_client_ = false;
  bool fail_make_current_ = false;
  int make_current_calls_ = 0;
};

// Test fixture wiring mock GL/EGL bindings and a single-threaded task
// environment to exercise GLSurfacePresentationHelper fallback paths.
//
// Lifetime and ownership: Instantiated per GoogleTest case; owns
// MockGLInterface and RealEGLApi for the duration of SetUp()..TearDown().
// Threading model: Fixture setup/teardown and helper calls run on the test
// main thread; static fake EGL sync callbacks use atomics so worker-thread
// eglClientWaitSyncKHR calls are thread-safe.
class GLSurfacePresentationHelperTest : public testing::Test {
 public:
  void SetUp() override {
    ResetFakeEGLState();

    SetGLGetProcAddressProc(MockGLInterface::GetGLProcAddress);
    display_ = GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<::testing::NiceMock<MockGLInterface>>();
    MockGLInterface::SetGLInterface(gl_.get());

    egl_api_ = std::make_unique<RealEGLApi>();
    egl_api_->Initialize(&g_driver_egl);
    g_current_egl_context = egl_api_.get();
    g_driver_egl.fn.eglGetCurrentDisplayFn = &FakeGetCurrentDisplay;
    g_driver_egl.fn.eglCreateSyncKHRFn = &FakeCreateSyncKHR;
    g_driver_egl.fn.eglClientWaitSyncKHRFn = &FakeClientWaitSyncKHR;
    g_driver_egl.fn.eglDestroySyncKHRFn = &FakeDestroySyncKHR;
    g_driver_egl.fn.eglGetSyncAttribKHRFn = &FakeGetSyncAttribKHR;
    g_driver_egl.fn.eglGetErrorFn = &FakeGetError;

    display_egl_ = display_->GetAs<GLDisplayEGL>();
    ASSERT_NE(nullptr, display_egl_);
    display_egl_->SetDisplay(FakeGetCurrentDisplay());
    display_egl_->ext->b_EGL_KHR_fence_sync = true;
    display_egl_->ext->b_EGL_KHR_wait_sync = true;
    display_egl_->ext->b_EGL_ANGLE_global_fence_sync = false;

    context_ = base::MakeRefCounted<TestGLContextWithEGLDisplay>(display_egl_);
    context_->SetExtensionsString("GL_EXT_disjoint_timer_query");
    context_->SetGLVersionString("OpenGL ES 3.0");
    surface_ = base::MakeRefCounted<GLSurfaceStub>();
    ASSERT_TRUE(context_->MakeCurrent(surface_.get()));
    gpu_timing_fake_.Reset();
  }

  void TearDown() override {
    context_ = nullptr;
    surface_ = nullptr;
    display_egl_ = nullptr;
    g_current_egl_context = nullptr;
    egl_api_.reset();
    g_driver_egl.ClearBindings();
    MockGLInterface::SetGLInterface(nullptr);
    GLSurfaceTestSupport::ShutdownGL(display_);
    display_ = nullptr;
    gl_.reset();
    ResetFakeEGLState();
  }

 protected:
  static void ResetFakeEGLState() {
    next_sync_id_.store(1, std::memory_order_relaxed);
    created_syncs_.store(0, std::memory_order_relaxed);
    destroyed_syncs_.store(0, std::memory_order_relaxed);
    wait_calls_.store(0, std::memory_order_relaxed);
    block_fence_signal_.store(false, std::memory_order_relaxed);
    fail_client_wait_.store(false, std::memory_order_relaxed);
    fence_signal_delay_ms_.store(0, std::memory_order_relaxed);
    last_fence_signaled_ticks_us_.store(0, std::memory_order_relaxed);
    fence_signaled_event_ = nullptr;
  }

  static EGLDisplay GL_BINDING_CALL FakeGetCurrentDisplay() {
    return reinterpret_cast<EGLDisplay>(0x1);
  }

  static EGLSyncKHR GL_BINDING_CALL FakeCreateSyncKHR(EGLDisplay,
                                                      EGLenum,
                                                      const EGLint*) {
    created_syncs_.fetch_add(1, std::memory_order_relaxed);
    const uintptr_t id = next_sync_id_.fetch_add(1, std::memory_order_relaxed);
    return reinterpret_cast<EGLSyncKHR>(id);
  }

  static EGLint GL_BINDING_CALL FakeClientWaitSyncKHR(EGLDisplay,
                                                      EGLSyncKHR,
                                                      EGLint,
                                                      EGLTimeKHR timeout_ns) {
    wait_calls_.fetch_add(1, std::memory_order_relaxed);
    if (fail_client_wait_.load(std::memory_order_acquire)) {
      return EGL_FALSE;
    }
    if (block_fence_signal_.load(std::memory_order_acquire)) {
      base::PlatformThread::Sleep(base::Nanoseconds(
          std::min<EGLTimeKHR>(timeout_ns, 1'000'000 /* 1ms */)));
      return EGL_TIMEOUT_EXPIRED_KHR;
    }
    const int delay_ms = fence_signal_delay_ms_.load(std::memory_order_relaxed);
    if (delay_ms > 0) {
      base::PlatformThread::Sleep(base::Milliseconds(delay_ms));
    }
    const int64_t signaled_us =
        (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds();
    last_fence_signaled_ticks_us_.store(signaled_us, std::memory_order_release);
    if (base::WaitableEvent* event =
            fence_signaled_event_.load(std::memory_order_acquire)) {
      event->Signal();
    }
    return EGL_CONDITION_SATISFIED_KHR;
  }

  static EGLBoolean GL_BINDING_CALL FakeDestroySyncKHR(EGLDisplay, EGLSyncKHR) {
    destroyed_syncs_.fetch_add(1, std::memory_order_relaxed);
    return EGL_TRUE;
  }

  static EGLBoolean GL_BINDING_CALL FakeGetSyncAttribKHR(EGLDisplay,
                                                         EGLSyncKHR,
                                                         EGLint,
                                                         EGLint* value) {
    *value = EGL_SIGNALED_KHR;
    return EGL_TRUE;
  }

  static EGLint GL_BINDING_CALL FakeGetError() { return EGL_SUCCESS; }

  std::unique_ptr<::testing::NiceMock<MockGLInterface>> gl_;
  std::unique_ptr<RealEGLApi> egl_api_;
  raw_ptr<GLDisplay> display_ = nullptr;
  raw_ptr<GLDisplayEGL> display_egl_ = nullptr;
  scoped_refptr<TestGLContextWithEGLDisplay> context_;
  scoped_refptr<GLSurfaceStub> surface_;
  GPUTimingFake gpu_timing_fake_;

  static std::atomic<uintptr_t> next_sync_id_;
  static std::atomic<int> created_syncs_;
  static std::atomic<int> destroyed_syncs_;
  static std::atomic<int> wait_calls_;
  static std::atomic<bool> block_fence_signal_;
  static std::atomic<bool> fail_client_wait_;
  static std::atomic<int> fence_signal_delay_ms_;
  static std::atomic<int64_t> last_fence_signaled_ticks_us_;
  static std::atomic<base::WaitableEvent*> fence_signaled_event_;
};

std::atomic<uintptr_t> GLSurfacePresentationHelperTest::next_sync_id_{1};
std::atomic<int> GLSurfacePresentationHelperTest::created_syncs_{0};
std::atomic<int> GLSurfacePresentationHelperTest::destroyed_syncs_{0};
std::atomic<int> GLSurfacePresentationHelperTest::wait_calls_{0};
std::atomic<bool> GLSurfacePresentationHelperTest::block_fence_signal_{false};
std::atomic<bool> GLSurfacePresentationHelperTest::fail_client_wait_{false};
std::atomic<int> GLSurfacePresentationHelperTest::fence_signal_delay_ms_{0};
std::atomic<int64_t>
    GLSurfacePresentationHelperTest::last_fence_signaled_ticks_us_{0};
std::atomic<base::WaitableEvent*>
    GLSurfacePresentationHelperTest::fence_signaled_event_{nullptr};

// When video playback or UI rendering runs on GPU drivers that truncate
// GL_TIMESTAMP_EXT query results to float32 at high uptime (e.g. Mali after
// 68.7s), GLSurfacePresentationHelper switches from GPUTimer to GLFence after 6
// frames without failing presentation or causing playback stutter.
TEST_F(GLSurfacePresentationHelperTest,
       FallsBackFromFloat32QuantizedGPUTimerToSubMsAsyncEGLFence) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLDeleteQueries));

  int query_counter_calls = 0;
  int64_t current_ns = 31LL * 3600 * base::Time::kNanosecondsPerSecond + 9999;
  uint64_t truncated_query_ns = 0;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT))
      .WillRepeatedly(Invoke([&](GLuint, GLenum) {
        ++query_counter_calls;
        truncated_query_ns =
            static_cast<uint64_t>(static_cast<float>(current_ns));
      }));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(
          Invoke([&](GLenum, GLint64* data) { *data = current_ns; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke([&](GLuint, GLenum, GLuint64* params) {
        *params = truncated_query_ns;
      }));

  // Bind GPUTiming's fake CPU clock so simulated 31h timestamps match CPU time.
  scoped_refptr<GPUTimingClient> timing_setup =
      context_->CreateGPUTimingClient();
  timing_setup->SetCpuTimeForTesting(
      base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));
  gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  // First 6 frames run on GPUTimer until float32 quantization is confirmed.
  for (int i = 0; i < 6; ++i) {
    gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);
    base::RunLoop loop;
    gfx::PresentationFeedback received;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            received = fb;
            loop.Quit();
          }));
    }
    current_ns += 8'000'000;
    gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);
    loop.Run();
    EXPECT_FALSE(received.failed());
  }
  EXPECT_EQ(6, query_counter_calls);
  EXPECT_EQ(0, created_syncs_.load());

  // Frame 7 must fall back to off-thread GLFenceEGL waiting and stamp the
  // timestamp when the worker wakes up (not when the GPU thread later runs).
  base::WaitableEvent signaled_event;
  fence_signaled_event_.store(&signaled_event, std::memory_order_release);
  fence_signal_delay_ms_.store(2, std::memory_order_relaxed);
  base::RunLoop fence_loop;
  gfx::PresentationFeedback fence_feedback;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          fence_feedback = fb;
          fence_loop.Quit();
        }));
    // Worker wait must not start during PreSwapBuffers before eglSwapBuffers
    // and PostSwapBuffers complete.
    EXPECT_EQ(0, wait_calls_.load());
  }
  signaled_event.Wait();
  // Yield briefly so WaitOnWorker records completion_ticks_us_, then block
  // the GPU thread for 20ms so any main-thread Now() poll would be >=20ms
  // later than the worker's completion timestamp.
  base::PlatformThread::Sleep(base::Milliseconds(20));
  const base::TimeTicks after_gpu_sleep = base::TimeTicks::Now();
  fence_signaled_event_.store(nullptr, std::memory_order_release);
  fence_loop.Run();

  EXPECT_EQ(6, query_counter_calls);
  EXPECT_EQ(1, created_syncs_.load());
  EXPECT_GE(wait_calls_.load(), 1);
  EXPECT_EQ(1, destroyed_syncs_.load());
  EXPECT_FALSE(fence_feedback.failed());

  const base::TimeTicks expected_signal_time =
      base::TimeTicks() + base::Microseconds(last_fence_signaled_ticks_us_.load(
                              std::memory_order_acquire));
  EXPECT_GE(fence_feedback.timestamp, expected_signal_time);
  EXPECT_LT(fence_feedback.timestamp, after_gpu_sleep - base::Milliseconds(10));
}

// When starting video playback or UI rendering on platforms with unpatched Mali
// GPU drivers where glGetInteger64v(GL_TIMESTAMP_EXT) and query results have
// mismatched timebases (~7113s skew), GLSurfacePresentationHelper discards the
// skewed timestamps in favor of TimeTicks::Now() and falls back to GLFence
// after 2 frames so playback telemetry does not record multi-hour frame
// intervals.
TEST_F(GLSurfacePresentationHelperTest,
       FallsBackFromCalibrationMismatchedGPUTimerToGLFence) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLDeleteQueries));

  // Simulate unpatched Mali where glGetInteger64v(GL_TIMESTAMP_EXT) returns raw
  // 24 MHz crystal ticks while GL_QUERY_RESULT is in nanoseconds (~7113s skew).
  int query_counter_calls = 0;
  int64_t current_ns = 3600LL * base::Time::kNanosecondsPerSecond + 777;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT))
      .WillRepeatedly(Invoke([&](GLuint, GLenum) { ++query_counter_calls; }));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(Invoke(
          [&](GLenum, GLint64* data) { *data = (current_ns * 24) / 1000; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke([&](GLuint, GLenum, GLuint64* params) {
        *params = static_cast<GLuint64>(current_ns);
      }));

  scoped_refptr<GPUTimingClient> timing_setup =
      context_->CreateGPUTimingClient();
  timing_setup->SetCpuTimeForTesting(
      base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));
  gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  // First 2 frames detect calibration mismatch and must discard the skewed
  // ~7113s GPU timestamp in favor of TimeTicks::Now().
  for (int i = 0; i < 2; ++i) {
    gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);
    const base::TimeTicks before = base::TimeTicks::Now();
    base::RunLoop loop;
    gfx::PresentationFeedback received;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            received = fb;
            loop.Quit();
          }));
    }
    current_ns += 16'667'000;
    gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);
    loop.Run();
    const base::TimeTicks after = base::TimeTicks::Now();
    EXPECT_FALSE(received.failed());
    EXPECT_GE(received.timestamp, before);
    EXPECT_LE(received.timestamp, after);
  }
  EXPECT_EQ(2, query_counter_calls);
  EXPECT_EQ(0, created_syncs_.load());

  // Frame 3 must permanently switch to GLFence.
  base::RunLoop fence_loop;
  gfx::PresentationFeedback fence_feedback;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          fence_feedback = fb;
          fence_loop.Quit();
        }));
  }
  fence_loop.Run();

  EXPECT_EQ(2, query_counter_calls);
  EXPECT_EQ(1, created_syncs_.load());
  EXPECT_GE(wait_calls_.load(), 1);
  EXPECT_EQ(1, destroyed_syncs_.load());
  EXPECT_FALSE(fence_feedback.failed());
}

// Verifies that when UI/video rendering pipelines multiple swaps before
// CheckPendingFrames() runs on a driver with uncalibrated GL_TIMESTAMP_EXT
// units, skewed multi-hour GPU timestamps are discarded for every queued frame
// so presentation/FPS telemetry does not see a bogus interval spike on the
// second frame.
TEST_F(GLSurfacePresentationHelperTest,
       DiscardsSkewedTimestampsAcrossMultipleQueuedFrames) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLDeleteQueries));

  int query_counter_calls = 0;
  int64_t current_ns = 3600LL * base::Time::kNanosecondsPerSecond + 777;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT))
      .WillRepeatedly(Invoke([&](GLuint, GLenum) { ++query_counter_calls; }));
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(Invoke(
          [&](GLenum, GLint64* data) { *data = (current_ns * 24) / 1000; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke([&](GLuint, GLenum, GLuint64* params) {
        *params = static_cast<GLuint64>(current_ns);
      }));

  scoped_refptr<GPUTimingClient> timing_setup =
      context_->CreateGPUTimingClient();
  timing_setup->SetCpuTimeForTesting(
      base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));
  gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  const base::TimeTicks before = base::TimeTicks::Now();
  base::RunLoop loop;
  gfx::PresentationFeedback fb1;
  gfx::PresentationFeedback fb2;
  int callbacks = 0;
  auto on_present = [&](gfx::PresentationFeedback* out,
                        const gfx::PresentationFeedback& fb) {
    *out = fb;
    if (++callbacks == 2) {
      loop.Quit();
    }
  };
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap1(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          on_present(&fb1, fb);
        }));
  }
  current_ns += 16'667'000;
  gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap2(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          on_present(&fb2, fb);
        }));
  }
  loop.Run();
  const base::TimeTicks after = base::TimeTicks::Now();

  EXPECT_FALSE(fb1.failed());
  EXPECT_FALSE(fb2.failed());
  EXPECT_EQ(2, query_counter_calls);
  EXPECT_GE(fb1.timestamp, before);
  EXPECT_LE(fb1.timestamp, after);
  EXPECT_GE(fb2.timestamp, before);
  EXPECT_LE(fb2.timestamp, after);
}

// When video playback initializes on contexts where GPU timing is unsupported
// (e.g. stub contexts returning null GPUTimingClient),
// GLSurfacePresentationHelper falls back cleanly to GLFence without crashing.
TEST_F(GLSurfacePresentationHelperTest,
       HandlesNullGPUTimingClientByFallingBackToGLFence) {
  context_->set_return_null_gpu_timing_client(true);

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  base::RunLoop fence_loop;
  gfx::PresentationFeedback fence_feedback;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          fence_feedback = fb;
          fence_loop.Quit();
        }));
  }
  fence_loop.Run();

  EXPECT_EQ(1, created_syncs_.load());
  EXPECT_GE(wait_calls_.load(), 1);
  EXPECT_EQ(1, destroyed_syncs_.load());
  EXPECT_FALSE(fence_feedback.failed());
}

// Verifies that when the surface/helper is destroyed while an async EGLSyncKHR
// wait is in flight (e.g. during Cobalt suspend/conceal before eglTerminate),
// ~GLSurfacePresentationHelper() synchronously cancels and destroys the
// EGLSyncKHR handle before returning.
TEST_F(GLSurfacePresentationHelperTest,
       CancelsAndDestroysPendingAsyncFenceBeforeDestructorReturns) {
  // Disable GL_TIMESTAMP_EXT so GLSurfacePresentationHelper uses async EGL
  // fences immediately.
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(0), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));

  block_fence_signal_.store(true, std::memory_order_release);
  bool failed_callback_ran = false;
  {
    GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
    helper.OnMakeCurrent(context_.get(), surface_.get());
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            EXPECT_TRUE(fb.failed());
            failed_callback_ran = true;
          }));
    }
    EXPECT_EQ(1, created_syncs_.load());
  }

  // ~GLSurfacePresentationHelper() must synchronously cancel the worker and
  // destroy the EGLSyncKHR before returning (safe against subsequent
  // eglTerminate).
  EXPECT_TRUE(failed_callback_ran);
  EXPECT_EQ(1, destroyed_syncs_.load());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, destroyed_syncs_.load());
}

// Verifies that if eglClientWaitSyncKHR returns EGL_FALSE on the worker thread,
// the helper logs and completes presentation without hitting a fatal NOTREACHED
// crash.
TEST_F(GLSurfacePresentationHelperTest, HandlesClientWaitErrorWithoutCrashing) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(0), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));

  fail_client_wait_.store(true, std::memory_order_release);
  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  const base::TimeTicks before =
      base::TimeTicks() +
      base::Microseconds(
          (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds());
  base::RunLoop loop;
  gfx::PresentationFeedback received;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          received = fb;
          loop.Quit();
        }));
  }
  loop.Run();
  const base::TimeTicks after = base::TimeTicks::Now();

  EXPECT_EQ(1, created_syncs_.load());
  EXPECT_EQ(1, wait_calls_.load());
  EXPECT_EQ(1, destroyed_syncs_.load());
  EXPECT_FALSE(received.failed());
  EXPECT_GE(received.timestamp, before);
  EXPECT_LE(received.timestamp, after);
}

// Verifies that when eglSwapBuffers fails (SWAP_FAILED), PostSwapBuffers
// immediately cancels and destroys the fence without dispatching a worker wait
// and reports PresentationFeedback::Failure().
TEST_F(GLSurfacePresentationHelperTest, SwapFailedCancelsPendingAsyncFence) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(0), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));

  block_fence_signal_.store(true, std::memory_order_release);
  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  base::RunLoop loop;
  gfx::PresentationFeedback received;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          received = fb;
          loop.Quit();
        }));
    swap.set_result(gfx::SwapResult::SWAP_FAILED);
  }
  loop.Run();

  EXPECT_TRUE(received.failed());
  EXPECT_EQ(1, created_syncs_.load());
  EXPECT_EQ(0, wait_calls_.load());
  EXPECT_EQ(1, destroyed_syncs_.load());
}

// Verifies that completing an async EGLSyncKHR wait does not invoke
// GLContext::MakeCurrent(surface_) on the GPU thread when no GL queries or
// synchronous GLFence objects are pending, preventing async fence completion
// callbacks from mutating GL context bindings or triggering spurious context
// loss between swaps.
TEST_F(GLSurfacePresentationHelperTest,
       AsyncFenceCompletionDoesNotCallMakeCurrentOnGPUThread) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(0), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  const int calls_before_swap = context_->make_current_calls();
  // Even if MakeCurrent would fail (e.g. another surface/context is active or
  // surface is being torn down), purely atomic FenceWaitState completion does
  // not touch GL state and must not call MakeCurrent.
  context_->set_fail_make_current(true);

  base::RunLoop loop;
  gfx::PresentationFeedback received;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          received = fb;
          loop.Quit();
        }));
  }
  loop.Run();

  EXPECT_FALSE(received.failed());
  EXPECT_EQ(calls_before_swap, context_->make_current_calls());
}

// Verifies that when a queued GPUTimer frame triggers CheckAndResetTimerErrors
// while a subsequent FenceWaitState frame is already queued behind it in
// pending_frames_, CheckPendingFrames() drains both the flushed timer frame and
// the completed async fence frame in FIFO order without stranding the fence
// callback.
TEST_F(GLSurfacePresentationHelperTest,
       DrainsMixedTimerErrorAndAsyncFenceQueueWithoutStrandingCallback) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLDeleteQueries));

  int64_t current_ns = 3600LL * base::Time::kNanosecondsPerSecond + 777;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT)).Times(3);
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(Invoke(
          [&](GLenum, GLint64* data) { *data = (current_ns * 24) / 1000; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke([&](GLuint, GLenum, GLuint64* params) {
        *params = static_cast<GLuint64>(current_ns);
      }));

  scoped_refptr<GPUTimingClient> timing_setup =
      context_->CreateGPUTimingClient();
  timing_setup->SetCpuTimeForTesting(
      base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));
  gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  // Complete frame 1 so calibration_error_streak_ reaches 1.
  {
    base::RunLoop loop1;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap1(
          &helper,
          base::BindLambdaForTesting(
              [&](const gfx::PresentationFeedback&) { loop1.Quit(); }));
    }
    current_ns += 16'667'000;
    gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);
    loop1.Run();
  }

  // Queue frame 2 (still uses GPUTimer since streak == 1 < 2). Before
  // CheckPendingFrames() runs for frame 2, trigger a second calibration-skewed
  // query via timing_setup so AreTimestampQueriesPrecise() flips to false while
  // frame 2's timer is still in pending_frames_. Then queue frame 3, which
  // creates an async FenceWaitState behind frame 2's timer.
  base::RunLoop both_done;
  std::vector<int> completion_order;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap2(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          EXPECT_FALSE(fb.failed());
          completion_order.push_back(2);
          if (completion_order.size() == 2u) {
            both_done.Quit();
          }
        }));
  }

  current_ns += 16'667'000;
  gpu_timing_fake_.SetCurrentCPUTime(current_ns / 1000);
  std::unique_ptr<GPUTimer> flip_timer = timing_setup->CreateGPUTimer(false);
  flip_timer->QueryTimeStamp();
  ASSERT_TRUE(flip_timer->IsAvailable());
  flip_timer->Destroy(true);
  ASSERT_FALSE(timing_setup->AreTimestampQueriesPrecise());

  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap3(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          EXPECT_FALSE(fb.failed());
          completion_order.push_back(3);
          if (completion_order.size() == 2u) {
            both_done.Quit();
          }
        }));
  }

  both_done.Run();
  ASSERT_EQ(2u, completion_order.size());
  EXPECT_EQ(2, completion_order[0]);
  EXPECT_EQ(3, completion_order[1]);
  EXPECT_EQ(1, created_syncs_.load());
  EXPECT_EQ(1, destroyed_syncs_.load());
}

// Covers buffered TV UI animations where two fast 6 ms frames pre-fill the EGL
// surface buffer queue followed by a 24 ms spike and a 50 ms genuine stall:
// pre-buffered frames must defer callback delivery until their FIFO VSync slot
// arrives (without calling MakeCurrent) and preserve writes_done_timestamp,
// while genuine buffer queue underruns report the true >33 ms dropped interval.
TEST_F(GLSurfacePresentationHelperTest,
       SpacesPreBufferedFramesOntoMonotonicFIFOVSyncSlotsAndPreservesWritesDone) {
  EXPECT_CALL(
      *gl_, GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(64), Return()));
  EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return()));
  EXPECT_CALL(*gl_, GenQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLGenQueries));
  EXPECT_CALL(*gl_, DeleteQueries(1, NotNull()))
      .WillRepeatedly(
          Invoke(&gpu_timing_fake_, &GPUTimingFake::FakeGLDeleteQueries));

  int64_t gpu_ns = 1'000'000'000LL;
  EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP_EXT)).Times(::testing::AnyNumber());
  EXPECT_CALL(*gl_, GetQueryObjectuiv(_, GL_QUERY_RESULT_AVAILABLE, NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<2>(1), Return()));
  EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP_EXT, NotNull()))
      .WillRepeatedly(Invoke([&](GLenum, GLint64* data) { *data = gpu_ns; }));
  EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
      .WillRepeatedly(Invoke([&](GLuint, GLenum, GLuint64* params) {
        *params = static_cast<GLuint64>(gpu_ns);
      }));

  base::SimpleTestTickClock test_clock;
  test_clock.Advance(base::Seconds(1));
  scoped_refptr<GPUTimingClient> timing_setup =
      context_->CreateGPUTimingClient();
  timing_setup->SetCpuTimeForTesting(
      base::BindRepeating(&GPUTimingFake::GetFakeCPUTime));
  gpu_timing_fake_.SetCurrentCPUTime(gpu_ns / 1000);

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.SetTickClockForTesting(&test_clock);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  constexpr base::TimeTicks kOrigin = base::TimeTicks() + base::Seconds(1);
  constexpr base::TimeDelta kVSync = base::Seconds(1) / 60;

  // Frame 1 finishes at t = 1000 ms.
  gfx::PresentationFeedback fb1;
  {
    base::RunLoop loop;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            fb1 = fb;
            loop.Quit();
          }));
    }
    loop.Run();
  }
  EXPECT_EQ(kOrigin, fb1.timestamp);
  EXPECT_EQ(kOrigin, fb1.writes_done_timestamp);
  EXPECT_EQ(kVSync, fb1.interval);

  // Frame 2 finishes rendering into a free back buffer 6 ms later (t = 1006 ms),
  // ahead of its FIFO VSync slot at 1016.666 ms. Once the deferred callback
  // runs at next_slot, timestamp must equal fb1.timestamp + kVSync (<= Now)
  // while writes_done_timestamp preserves the 1006 ms GPU completion time.
  gpu_ns += 6'000'000;
  gpu_timing_fake_.SetCurrentCPUTime(gpu_ns / 1000);
  test_clock.Advance(base::Milliseconds(6));
  gfx::PresentationFeedback fb2;
  bool fb2_ran = false;
  base::RunLoop fb2_loop;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          fb2 = fb;
          fb2_ran = true;
          fb2_loop.Quit();
        }));
  }
  // Run immediate check: reads GPUTimer once and defers callback until
  // next_slot because NowTicks() (1006 ms) < next_slot (1016.666 ms).
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fb2_ran);

  const int make_current_before_deferred = context_->make_current_calls();
  test_clock.Advance(kVSync - base::Milliseconds(6));
  fb2_loop.Run();
  ASSERT_TRUE(fb2_ran);
  EXPECT_EQ(fb1.timestamp + kVSync, fb2.timestamp);
  EXPECT_EQ(kOrigin + base::Milliseconds(6), fb2.writes_done_timestamp);
  // Cached GPU completion must not rebind the GL context on deferred wakeup.
  EXPECT_EQ(0, context_->make_current_calls() - make_current_before_deferred);

  // Frame 3 spikes to 24 ms from 1006 ms -> 1030 ms (still beating its FIFO
  // VSync slot at fb2.timestamp + kVSync = 1033.332 ms).
  gpu_ns += 24'000'000;
  gpu_timing_fake_.SetCurrentCPUTime(gpu_ns / 1000);
  test_clock.SetNowTicks(kOrigin + base::Milliseconds(34));
  gfx::PresentationFeedback fb3;
  {
    base::RunLoop loop;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            fb3 = fb;
            loop.Quit();
          }));
    }
    loop.Run();
  }
  EXPECT_EQ(fb2.timestamp + kVSync, fb3.timestamp);
  EXPECT_EQ(kOrigin + base::Milliseconds(30), fb3.writes_done_timestamp);

  // Frame 4 stalls for 50 ms (1030 -> 1080 ms, missing the 1050 ms VSync slot):
  // must report the real underrun timestamp rather than masking the drop.
  gpu_ns += 50'000'000;
  gpu_timing_fake_.SetCurrentCPUTime(gpu_ns / 1000);
  test_clock.SetNowTicks(kOrigin + base::Milliseconds(80));
  gfx::PresentationFeedback fb4;
  {
    base::RunLoop loop;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            fb4 = fb;
            loop.Quit();
          }));
    }
    loop.Run();
  }
  EXPECT_EQ(kOrigin + base::Milliseconds(80), fb4.timestamp);
  EXPECT_GT(fb4.timestamp - fb3.timestamp, base::Milliseconds(40));
}

// Covers 50 Hz PAL/European TV displays without a VSyncProvider: verifies both
// runtime calibration from 16 consistent 20 ms full-queue PostSwapBuffers
// intervals (Option B) and immediate override when the platform GLSurface
// reports a nominal VSync interval (Option A).
TEST_F(GLSurfacePresentationHelperTest,
       Calibrates50HzNominalIntervalFromBlockingSwapCadenceAndPlatformSurface) {
  context_->set_return_null_gpu_timing_client(true);

  base::SimpleTestTickClock test_clock;
  test_clock.Advance(base::Seconds(10));

  GLSurfacePresentationHelper helper(nullptr /* vsync_provider */);
  helper.SetTickClockForTesting(&test_clock);
  helper.OnMakeCurrent(context_.get(), surface_.get());

  gfx::PresentationFeedback last_fb;
  // Issue 17 swaps spaced by 20 ms (16 inter-swap intervals at 50 Hz).
  for (int i = 0; i < 17; ++i) {
    base::RunLoop loop;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            last_fb = fb;
            loop.Quit();
          }));
    }
    loop.Run();
    test_clock.Advance(base::Milliseconds(20));
  }
  EXPECT_EQ(base::Milliseconds(20), last_fb.interval);

  // If the platform GLSurface explicitly reports 60 Hz (Option A), it takes
  // precedence immediately without waiting for swap-cadence calibration.
  surface_->SetNominalVSyncInterval(base::Seconds(1) / 60);
  {
    base::RunLoop loop;
    {
      GLSurfacePresentationHelper::ScopedSwapBuffers swap(
          &helper,
          base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
            last_fb = fb;
            loop.Quit();
          }));
    }
    loop.Run();
  }
  EXPECT_EQ(base::Seconds(1) / 60, last_fb.interval);
  surface_->SetNominalVSyncInterval(base::TimeDelta());
}

}  // namespace
}  // namespace gl

#endif  // BUILDFLAG(IS_COBALT) && !BUILDFLAG(IS_ANDROID)

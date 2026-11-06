// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_presentation_helper.h"

#include <algorithm>
#include <atomic>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
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

#if !BUILDFLAG(IS_ANDROID)

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
  base::TimeTicks after_gpu_sleep;
  {
    GLSurfacePresentationHelper::ScopedSwapBuffers swap(
        &helper,
        base::BindLambdaForTesting([&](const gfx::PresentationFeedback& fb) {
          fence_feedback = fb;
          fence_loop.Quit();
        }));
    signaled_event.Wait();
    // Yield briefly so WaitOnWorker records completion_ticks_us_, then block
    // the GPU thread for 20ms so any main-thread Now() poll would be >=20ms
    // later than the worker's completion timestamp.
    base::PlatformThread::Sleep(base::Milliseconds(20));
    after_gpu_sleep = base::TimeTicks::Now();
  }
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
  EXPECT_EQ(1, destroyed_syncs_.load());
}

}  // namespace
}  // namespace gl

#endif  // !BUILDFLAG(IS_ANDROID)

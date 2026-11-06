// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_presentation_helper.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_COBALT) && !BUILDFLAG(IS_ANDROID)

#include <atomic>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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
};

std::atomic<uintptr_t> GLSurfacePresentationHelperTest::next_sync_id_{1};
std::atomic<int> GLSurfacePresentationHelperTest::created_syncs_{0};
std::atomic<int> GLSurfacePresentationHelperTest::destroyed_syncs_{0};

// When video playback or UI rendering runs on GPU drivers that truncate
// GL_TIMESTAMP_EXT query results to float32 at high uptime (e.g. Mali after
// 68.7s), GLSurfacePresentationHelper switches from GPUTimer to GLFence after 6
// frames without failing presentation or causing playback stutter.
TEST_F(GLSurfacePresentationHelperTest,
       FallsBackFromFloat32QuantizedGPUTimerToGLFence) {
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

  // Frame 7 must fall back to GLFence without issuing further timestamp
  // queries.
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

  EXPECT_EQ(6, query_counter_calls);
  EXPECT_EQ(1, created_syncs_.load());
  EXPECT_EQ(1, destroyed_syncs_.load());
  EXPECT_FALSE(fence_feedback.failed());
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
  EXPECT_EQ(1, destroyed_syncs_.load());
  EXPECT_FALSE(fence_feedback.failed());
}

}  // namespace
}  // namespace gl

#endif  // BUILDFLAG(IS_COBALT) && !BUILDFLAG(IS_ANDROID)

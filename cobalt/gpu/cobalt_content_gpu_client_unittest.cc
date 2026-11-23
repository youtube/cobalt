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

#include "cobalt/gpu/cobalt_content_gpu_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_utils.h"

namespace cobalt {
namespace {

const EGLDisplay kFakeDisplay = reinterpret_cast<EGLDisplay>(0x1);

enum class EglCallType {
  kMakeCurrent,
  kTerminate,
};

struct RecordedEglCall {
  EglCallType type;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface draw = EGL_NO_SURFACE;
  EGLSurface read = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  base::PlatformThreadId thread_id = base::kInvalidThreadId;
};

// Simulates an in-process GPU thread by invoking
// CobaltContentGpuClient::PostSandboxInitialized() during base::Thread::Init().
//
// Lifetime and Ownership:
// Stack-owned by individual test cases on the main test thread; holds a
// non-owning pointer to the test's CobaltContentGpuClient, which outlives this
// thread.
//
// Threading Model:
// Start() and Stop() are called on the main test thread, while Init() and
// sequence-local teardown execute on the spawned GPU thread.
class TestGpuThread : public base::Thread {
 public:
  explicit TestGpuThread(CobaltContentGpuClient* gpu_client,
                         base::OnceClosure on_cleanup = {})
      : base::Thread("TestGpuThread"),
        gpu_client_(gpu_client),
        on_cleanup_(std::move(on_cleanup)) {}

  TestGpuThread(const TestGpuThread&) = delete;
  TestGpuThread& operator=(const TestGpuThread&) = delete;

  ~TestGpuThread() override { Stop(); }

 protected:
  void Init() override { gpu_client_->PostSandboxInitialized(); }
  void CleanUp() override {
    if (on_cleanup_) {
      std::move(on_cleanup_).Run();
    }
  }

 private:
  raw_ptr<CobaltContentGpuClient> gpu_client_;
  base::OnceClosure on_cleanup_;
};

// Test fixture that intercepts global EGL driver bindings to verify that
// CobaltContentGpuClient shuts down an active default EGLDisplay when the GPU
// thread exits.
//
// Lifetime and Ownership:
// Created and destroyed per test by GoogleTest on the main test thread.
//
// Threading Model:
// SetUp(), test assertions, and TearDown() run on the main test thread.
// FakeMakeCurrent() and FakeTerminate() record calls on the spawned GPU thread
// and are inspected on the main test thread only after Stop() joins it.
class CobaltContentGpuClientTest : public testing::Test {
 protected:
  void SetUp() override {
    active_test_ = this;
    recorded_calls_.clear();

    saved_egl_context_ = gl::g_current_egl_context;
    saved_make_current_fn_ = gl::g_driver_egl.fn.eglMakeCurrentFn;
    saved_terminate_fn_ = gl::g_driver_egl.fn.eglTerminateFn;

    display_ = gl::GetDefaultDisplayEGL();
    ASSERT_NE(display_, nullptr);
    saved_display_ = display_->GetDisplay();

    gl::g_driver_egl.fn.eglMakeCurrentFn = &FakeMakeCurrent;
    gl::g_driver_egl.fn.eglTerminateFn = &FakeTerminate;

    egl_api_ = std::make_unique<gl::RealEGLApi>();
    egl_api_->Initialize(&gl::g_driver_egl);
    gl::g_current_egl_context = egl_api_.get();
  }

  void TearDown() override {
    if (display_) {
      display_->SetDisplay(saved_display_);
    }
    gl::g_current_egl_context = saved_egl_context_;
    gl::g_driver_egl.fn.eglMakeCurrentFn = saved_make_current_fn_;
    gl::g_driver_egl.fn.eglTerminateFn = saved_terminate_fn_;
    egl_api_.reset();
    recorded_calls_.clear();
    active_test_ = nullptr;
  }

  static EGLBoolean GL_BINDING_CALL FakeMakeCurrent(EGLDisplay dpy,
                                                    EGLSurface draw,
                                                    EGLSurface read,
                                                    EGLContext ctx) {
    if (active_test_) {
      active_test_->recorded_calls_.push_back(
          {EglCallType::kMakeCurrent, dpy, draw, read, ctx,
           base::PlatformThread::CurrentId()});
    }
    return EGL_TRUE;
  }

  static EGLBoolean GL_BINDING_CALL FakeTerminate(EGLDisplay dpy) {
    if (active_test_) {
      active_test_->recorded_calls_.push_back(
          {EglCallType::kTerminate, dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
           EGL_NO_CONTEXT, base::PlatformThread::CurrentId()});
    }
    return EGL_TRUE;
  }

  static CobaltContentGpuClientTest* active_test_;

  raw_ptr<gl::GLDisplayEGL> display_ = nullptr;
  EGLDisplay saved_display_ = EGL_NO_DISPLAY;
  raw_ptr<gl::EGLApi> saved_egl_context_ = nullptr;
  gl::eglMakeCurrentProc saved_make_current_fn_ = nullptr;
  gl::eglTerminateProc saved_terminate_fn_ = nullptr;
  std::unique_ptr<gl::RealEGLApi> egl_api_;
  std::vector<RecordedEglCall> recorded_calls_;
};

CobaltContentGpuClientTest* CobaltContentGpuClientTest::active_test_ = nullptr;

// Covers the app termination journey when Cobalt stops directly from preload or
// after unfreezing from the frozen state before the window is revealed, where
// OnConceal() does not run and the default EGLDisplay is still initialized when
// the in-process GPU thread stops.
TEST_F(CobaltContentGpuClientTest, ShutsDownActiveEGLDisplayOnGpuThreadStop) {
  display_->SetDisplay(kFakeDisplay);
  ASSERT_TRUE(display_->IsInitialized());

  CobaltContentGpuClient gpu_client;
  bool display_was_initialized_in_cleanup = false;
  bool had_egl_calls_in_cleanup = true;
  TestGpuThread gpu_thread(
      &gpu_client,
      base::BindOnce(
          [](gl::GLDisplayEGL* display,
             const std::vector<RecordedEglCall>* calls, bool* was_initialized,
             bool* had_calls) {
            *was_initialized = display->IsInitialized();
            *had_calls = !calls->empty();
          },
          display_.get(), &recorded_calls_, &display_was_initialized_in_cleanup,
          &had_egl_calls_in_cleanup));
  ASSERT_TRUE(gpu_thread.Start());
  const base::PlatformThreadId gpu_thread_id = gpu_thread.GetThreadId();
  ASSERT_NE(gpu_thread_id, base::kInvalidThreadId);
  ASSERT_NE(gpu_thread_id, base::PlatformThread::CurrentId());

  gpu_thread.Stop();

  EXPECT_TRUE(display_was_initialized_in_cleanup);
  EXPECT_FALSE(had_egl_calls_in_cleanup);
  EXPECT_FALSE(display_->IsInitialized());
  EXPECT_EQ(display_->GetDisplay(), EGL_NO_DISPLAY);
  ASSERT_EQ(recorded_calls_.size(), 2u);

  EXPECT_EQ(recorded_calls_[0].type, EglCallType::kMakeCurrent);
  EXPECT_EQ(recorded_calls_[0].display, kFakeDisplay);
  EXPECT_EQ(recorded_calls_[0].draw, EGL_NO_SURFACE);
  EXPECT_EQ(recorded_calls_[0].read, EGL_NO_SURFACE);
  EXPECT_EQ(recorded_calls_[0].context, EGL_NO_CONTEXT);
  EXPECT_EQ(recorded_calls_[0].thread_id, gpu_thread_id);

  EXPECT_EQ(recorded_calls_[1].type, EglCallType::kTerminate);
  EXPECT_EQ(recorded_calls_[1].display, kFakeDisplay);
  EXPECT_EQ(recorded_calls_[1].thread_id, gpu_thread_id);
}

// Covers the normal foreground-to-conceal-to-stop app termination journey where
// OnConceal() has already shut down the default EGLDisplay before the
// in-process GPU thread stops, ensuring eglTerminate() is not called twice.
TEST_F(CobaltContentGpuClientTest,
       SkipsEGLDisplayShutdownWhenAlreadyTerminatedByConceal) {
  display_->SetDisplay(EGL_NO_DISPLAY);
  ASSERT_FALSE(display_->IsInitialized());

  CobaltContentGpuClient gpu_client;
  TestGpuThread gpu_thread(&gpu_client);
  ASSERT_TRUE(gpu_thread.Start());
  gpu_thread.Stop();

  EXPECT_FALSE(display_->IsInitialized());
  EXPECT_TRUE(recorded_calls_.empty());
}

// Covers the GPU thread restart journey where the same CobaltContentGpuClient
// serves a newly spawned GPU thread after a prior GPU thread stopped, ensuring
// each GPU thread independently shuts down an active default EGLDisplay on
// exit.
TEST_F(CobaltContentGpuClientTest, ShutsDownEGLDisplayAcrossGpuThreadRestart) {
  CobaltContentGpuClient gpu_client;

  display_->SetDisplay(kFakeDisplay);
  {
    TestGpuThread first_gpu_thread(&gpu_client);
    ASSERT_TRUE(first_gpu_thread.Start());
    first_gpu_thread.Stop();
  }
  EXPECT_FALSE(display_->IsInitialized());
  EXPECT_EQ(recorded_calls_.size(), 2u);

  recorded_calls_.clear();
  display_->SetDisplay(kFakeDisplay);
  ASSERT_TRUE(display_->IsInitialized());

  TestGpuThread second_gpu_thread(&gpu_client);
  ASSERT_TRUE(second_gpu_thread.Start());
  const base::PlatformThreadId second_thread_id =
      second_gpu_thread.GetThreadId();
  ASSERT_NE(second_thread_id, base::kInvalidThreadId);
  second_gpu_thread.Stop();

  EXPECT_FALSE(display_->IsInitialized());
  ASSERT_EQ(recorded_calls_.size(), 2u);
  EXPECT_EQ(recorded_calls_[0].type, EglCallType::kMakeCurrent);
  EXPECT_EQ(recorded_calls_[0].thread_id, second_thread_id);
  EXPECT_EQ(recorded_calls_[1].type, EglCallType::kTerminate);
  EXPECT_EQ(recorded_calls_[1].thread_id, second_thread_id);
}

}  // namespace
}  // namespace cobalt

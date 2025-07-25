// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/test/rendering_test.h"

#include <memory>
#include <utility>

#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/gpu_service_webview.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/test/test_synchronous_compositor_android.h"

namespace android_webview {

namespace {
// BrowserViewRenderer subclass used for enabling tests to observe
// OnParentDrawDataUpdated.
class TestBrowserViewRenderer : public BrowserViewRenderer {
 public:
  TestBrowserViewRenderer(
      RenderingTest* rendering_test,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : BrowserViewRenderer(rendering_test, ui_task_runner),
        rendering_test_(rendering_test) {}

  ~TestBrowserViewRenderer() override {}

  void OnParentDrawDataUpdated(
      CompositorFrameConsumer* compositor_frame_consumer) override {
    BrowserViewRenderer::OnParentDrawDataUpdated(compositor_frame_consumer);
    rendering_test_->OnParentDrawDataUpdated();
  }

 private:
  const raw_ptr<RenderingTest> rendering_test_;
};
}  // namespace

RenderingTest::RenderingTest()
    : task_environment_(std::make_unique<base::test::TaskEnvironment>()) {
  ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  android_webview::GpuServiceWebView::GetInstance();
}

RenderingTest::~RenderingTest() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (window_.get())
    window_->Detach();
}

ui::TouchHandleDrawable* RenderingTest::CreateDrawable() {
  return nullptr;
}

void RenderingTest::SetUpTestHarness() {
  DCHECK(!browser_view_renderer_.get());
  DCHECK(!functor_.get());
  browser_view_renderer_ = std::make_unique<TestBrowserViewRenderer>(
      this, base::SingleThreadTaskRunner::GetCurrentDefault());
  browser_view_renderer_->SetActiveFrameSinkId(viz::FrameSinkId(1, 0));
  browser_view_renderer_->SetDipScale(1.0f);
  InitializeCompositor();
  std::unique_ptr<FakeWindow> window(
      new FakeWindow(browser_view_renderer_.get(), this, gfx::Rect(100, 100)));
  functor_ = std::make_unique<FakeFunctor>();
  functor_->Init(window.get(),
                 std::make_unique<RenderThreadManager>(
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
  browser_view_renderer_->SetCurrentCompositorFrameConsumer(
      functor_->GetCompositorFrameConsumer());
  window_ = std::move(window);
}

CompositorFrameConsumer* RenderingTest::GetCompositorFrameConsumer() {
  return functor_->GetCompositorFrameConsumer();
}

CompositorFrameProducer* RenderingTest::GetCompositorFrameProducer() {
  return browser_view_renderer_.get();
}

void RenderingTest::InitializeCompositor() {
  DCHECK(!compositor_.get());
  DCHECK(browser_view_renderer_.get());
  compositor_ = std::make_unique<content::TestSynchronousCompositor>(
      viz::FrameSinkId(1, 0));
  compositor_->SetClient(browser_view_renderer_.get());
}

void RenderingTest::RunTest() {
  SetUpTestHarness();

  ui_task_runner_->PostTask(FROM_HERE, base::BindOnce(&RenderingTest::StartTest,
                                                      base::Unretained(this)));
  run_loop_.Run();
}

void RenderingTest::StartTest() {
  EndTest();
}

void RenderingTest::EndTest() {
  run_loop_.QuitWhenIdle();
}

content::SynchronousCompositor* RenderingTest::ActiveCompositor() const {
  return browser_view_renderer_->GetActiveCompositorForTesting();
}

std::unique_ptr<viz::CompositorFrame> RenderingTest::ConstructEmptyFrame() {
  gfx::Rect viewport(browser_view_renderer_->size());
  return std::make_unique<viz::CompositorFrame>(
      viz::CompositorFrameBuilder()
          .AddRenderPass(viewport, viewport)
          .SetDeviceScaleFactor(1.0f)
          .Build());
}

std::unique_ptr<viz::CompositorFrame> RenderingTest::ConstructFrame(
    viz::ResourceId resource_id) {
  std::unique_ptr<viz::CompositorFrame> compositor_frame(ConstructEmptyFrame());
  viz::TransferableResource resource;
  resource.id = resource_id;
  compositor_frame->resource_list.push_back(resource);
  return compositor_frame;
}

FakeFunctor* RenderingTest::GetFunctor() {
  return functor_.get();
}

void RenderingTest::WillOnDraw() {
  DCHECK(compositor_);
  compositor_->SetHardwareFrame(0u, ConstructEmptyFrame());
}

bool RenderingTest::WillDrawOnRT(HardwareRendererDrawParams* params) {
  params->width = window_->surface_size().width();
  params->height = window_->surface_size().height();
  gfx::Transform transform;
  transform.GetColMajorF(params->transform);
  return true;
}

void RenderingTest::OnNewPicture() {}

void RenderingTest::PostInvalidate(bool inside_vsync) {
  if (window_)
    window_->PostInvalidate();
}

gfx::Point RenderingTest::GetLocationOnScreen() {
  return gfx::Point();
}

}  // namespace android_webview

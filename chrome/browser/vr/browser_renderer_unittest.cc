// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/browser_renderer.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/input_delegate.h"
#include "chrome/browser/vr/input_event.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/scheduler_delegate.h"
#include "chrome/browser/vr/test/mock_browser_ui_interface.h"
#include "chrome/browser/vr/ui_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Return;

namespace vr {

class MockUi : public UiInterface {
 public:
  MockUi() = default;
  ~MockUi() override = default;

  base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr() override {
    return nullptr;
  }
  SchedulerUiInterface* GetSchedulerUiPtr() override { return nullptr; }
  MOCK_METHOD0(OnGlInitialized, void());
  MOCK_METHOD0(OnPause, void());
  MOCK_METHOD2(GetTargetPointForTesting,
               gfx::Point3F(UserFriendlyElementName,
                            const gfx::PointF& position));
  MOCK_METHOD1(GetElementVisibilityForTesting, bool(UserFriendlyElementName));
  MOCK_METHOD2(OnBeginFrame, bool(base::TimeTicks, const gfx::Transform&));
  MOCK_CONST_METHOD0(SceneHasDirtyTextures, bool());
  MOCK_METHOD0(UpdateSceneTextures, void());
  MOCK_METHOD1(Draw, void(const RenderInfo&));
  MOCK_METHOD2(DrawWebXr, void(int, const float (&)[16]));
  MOCK_METHOD1(DrawWebVrOverlayForeground, void(const RenderInfo&));
  MOCK_METHOD0(HasWebXrOverlayElementsToDraw, bool());
  MOCK_METHOD1(HandleMenuButtonEvents, void(InputEventList*));
  FovRectangles GetMinimalFovForWebXrOverlayElements(const gfx::Transform&,
                                                     const FovRectangle&,
                                                     const gfx::Transform&,
                                                     const FovRectangle&,
                                                     float) override {
    return {};
  }
};

class MockSchedulerDelegate : public SchedulerDelegate {
 public:
  MockSchedulerDelegate() = default;
  ~MockSchedulerDelegate() override = default;

  // SchedulerDelegate
  void OnPause() override {}
  void OnResume() override {}
  MOCK_METHOD0(OnExitPresent, void());
  void SetBrowserRenderer(SchedulerBrowserRendererInterface*) override {}
  MOCK_METHOD2(SubmitDrawnFrame, void(FrameType, const gfx::Transform&));
  void AddInputSourceState(
      device::mojom::XRInputSourceStatePtr state) override {}
  void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options) override {}
};

class MockGraphicsDelegate : public GraphicsDelegate {
 public:
  MockGraphicsDelegate() = default;
  ~MockGraphicsDelegate() override = default;

  bool using_buffer() { return using_buffer_; }

  // GraphicsDelegate
  void OnResume() {}
  FovRectangles GetRecommendedFovs() override { return {}; }
  RenderInfo GetRenderInfo(FrameType, const gfx::Transform&) override {
    return {};
  }
  RenderInfo GetOptimizedRenderInfoForFovs(const FovRectangles&) override {
    return {};
  }
  void InitializeBuffers() override {}
  void PrepareBufferForWebXr() override { UseBuffer(); }
  void PrepareBufferForWebXrOverlayElements() override { UseBuffer(); }
  void PrepareBufferForBrowserUi() override { UseBuffer(); }
  void OnFinishedDrawingBuffer() override {
    CHECK(using_buffer_);
    using_buffer_ = false;
  }
  void GetWebXrDrawParams(int*, Transform*) override {}
  bool RunInSkiaContext(base::OnceClosure callback) override {
    std::move(callback).Run();
    return true;
  }

  // TODO(https://crbug.com/1493735): Provide implementations during refactor
  // as needed.
  bool PreRender() override { return true; }
  void PostRender() override {}
  mojo::PlatformHandle GetTexture() override { NOTREACHED_NORETURN(); }
  const gpu::SyncToken& GetSyncToken() override { NOTREACHED_NORETURN(); }
  void ResetMemoryBuffer() override {}
  bool BindContext() override { return true; }
  void ClearContext() override {}

 private:
  void UseBuffer() {
    CHECK(!using_buffer_);
    using_buffer_ = true;
  }
  bool using_buffer_ = false;
};

class MockInputDelegate : public InputDelegate {
 public:
  MockInputDelegate() = default;
  ~MockInputDelegate() override = default;

  // InputDelegate
  gfx::Transform GetHeadPose() override { return {}; }
  void OnTriggerEvent(bool pressed) override {}
  MOCK_METHOD3(UpdateController,
               void(const gfx::Transform&, base::TimeTicks, bool));
  MOCK_METHOD1(GetControllerModel, ControllerModel(const gfx::Transform&));
  MOCK_METHOD1(GetGestures, InputEventList(base::TimeTicks));
  device::mojom::XRInputSourceStatePtr GetInputSourceState() override {
    return nullptr;
  }
  void OnResume() override {}
  void OnPause() override {}
};

class BrowserRendererTest : public testing::Test {
 public:
  struct BuildParams {
    std::unique_ptr<MockUi> ui;
    std::unique_ptr<MockSchedulerDelegate> scheduler_delegate;
    std::unique_ptr<MockGraphicsDelegate> graphics_delegate;
    std::unique_ptr<MockInputDelegate> input_delegate;
  };

  void SetUp() override {
    auto ui = std::make_unique<MockUi>();
    build_params_ = {
        std::make_unique<MockUi>(), std::make_unique<MockSchedulerDelegate>(),
        std::make_unique<MockGraphicsDelegate>(),
        std::make_unique<MockInputDelegate>(),
    };
    ui_ = build_params_.ui.get();
    scheduler_delegate_ = build_params_.scheduler_delegate.get();
    graphics_delegate_ = build_params_.graphics_delegate.get();
    input_delegate_ = build_params_.input_delegate.get();
  }

  std::unique_ptr<SchedulerBrowserRendererInterface> CreateBrowserRenderer() {
    return std::make_unique<BrowserRenderer>(
        std::move(build_params_.ui),
        std::move(build_params_.scheduler_delegate),
        std::move(build_params_.graphics_delegate),
        std::move(build_params_.input_delegate), nullptr,
        1 /* sliding_time_size */);
  }

 protected:
  raw_ptr<MockUi, DanglingUntriaged> ui_;
  raw_ptr<MockSchedulerDelegate, DanglingUntriaged> scheduler_delegate_;
  raw_ptr<MockGraphicsDelegate, DanglingUntriaged> graphics_delegate_;
  raw_ptr<MockInputDelegate, DanglingUntriaged> input_delegate_;

 private:
  BuildParams build_params_;
};

TEST_F(BrowserRendererTest, DrawWebXrFrameNoOverlay) {
  testing::Sequence s;
  auto browser_renderer = CreateBrowserRenderer();
  EXPECT_CALL(*ui_, SceneHasDirtyTextures()).WillOnce(Return(false));
  EXPECT_CALL(*ui_, UpdateSceneTextures).Times(0);
  EXPECT_CALL(*ui_, HasWebXrOverlayElementsToDraw()).WillOnce(Return(false));

  // No input processing.
  testing::Expectation update_controller =
      EXPECT_CALL(*input_delegate_, UpdateController(_, _, _)).Times(0);
  EXPECT_CALL(*input_delegate_, GetGestures(_)).Times(0);
  EXPECT_CALL(*input_delegate_, GetControllerModel(_)).Times(0);
  EXPECT_CALL(*ui_, HandleMenuButtonEvents(_)).Times(0);

  EXPECT_CALL(*ui_, OnBeginFrame(_, _)).Times(1).InSequence(s);
  EXPECT_CALL(*ui_, Draw(_)).Times(0);
  EXPECT_CALL(*ui_, DrawWebXr(_, _)).Times(1).InSequence(s);
  EXPECT_CALL(*ui_, DrawWebVrOverlayForeground(_)).Times(0);
  EXPECT_CALL(*scheduler_delegate_, SubmitDrawnFrame(kWebXrFrame, _))
      .Times(1)
      .InSequence(s);

  browser_renderer->DrawWebXrFrame(base::TimeTicks(), {});
  EXPECT_FALSE(graphics_delegate_->using_buffer());
}

TEST_F(BrowserRendererTest, DrawWebXrFrameWithOverlay) {
  testing::Sequence s;
  auto browser_renderer = CreateBrowserRenderer();
  EXPECT_CALL(*ui_, SceneHasDirtyTextures()).WillOnce(Return(false));
  EXPECT_CALL(*ui_, UpdateSceneTextures).Times(0);
  EXPECT_CALL(*ui_, HasWebXrOverlayElementsToDraw()).WillOnce(Return(true));

  // No input processing.
  testing::Expectation update_controller =
      EXPECT_CALL(*input_delegate_, UpdateController(_, _, _)).Times(0);
  EXPECT_CALL(*input_delegate_, GetGestures(_)).Times(0);
  EXPECT_CALL(*input_delegate_, GetControllerModel(_)).Times(0);
  EXPECT_CALL(*ui_, HandleMenuButtonEvents(_)).Times(0);

  EXPECT_CALL(*ui_, OnBeginFrame(_, _)).Times(1).InSequence(s);
  EXPECT_CALL(*ui_, Draw(_)).Times(0);
  EXPECT_CALL(*ui_, DrawWebXr(_, _)).Times(1).InSequence(s);
  EXPECT_CALL(*ui_, DrawWebVrOverlayForeground(_)).Times(1).InSequence(s);
  EXPECT_CALL(*scheduler_delegate_, SubmitDrawnFrame(kWebXrFrame, _))
      .Times(1)
      .InSequence(s);

  browser_renderer->DrawWebXrFrame(base::TimeTicks(), {});
  EXPECT_FALSE(graphics_delegate_->using_buffer());
}

TEST_F(BrowserRendererTest, ProcessControllerInputForWebXr) {
  testing::Sequence s;
  auto browser_renderer = CreateBrowserRenderer();

  testing::Expectation update_controller =
      EXPECT_CALL(*input_delegate_, UpdateController(_, _, true))
          .Times(1)
          .InSequence(s);
  EXPECT_CALL(*input_delegate_, GetGestures(_)).Times(1).InSequence(s);
  EXPECT_CALL(*input_delegate_, GetControllerModel(_)).Times(0);
  EXPECT_CALL(*ui_, HandleMenuButtonEvents(_)).Times(1).InSequence(s);

  browser_renderer->ProcessControllerInputForWebXr(gfx::Transform(),
                                                   base::TimeTicks());
}

}  // namespace vr

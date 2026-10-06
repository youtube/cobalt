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

#include "base/test/test_simple_task_runner.h"
#include "base/unguessable_token.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/public/browser/media_service.h"
#include "content/public/renderer/render_frame_media_playback_options.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/starboard/starboard_renderer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

namespace {

// Scoped test helper that binds a media::mojom::MediaService receiver owning a
// media::StarboardRendererConcealRegistry and a media::StarboardRenderer on a
// manual base::TestSimpleTaskRunner so unit tests can deterministically step
// the media conceal barrier and verify conceal/reveal state across Mojo.
class FakeStarboardRendererBarrier final : public media::mojom::MediaService {
 public:
  FakeStarboardRendererBarrier() : renderer_(CreateRenderer()) {
    OverrideMediaServiceForTesting(remote_.get());
  }

  ~FakeStarboardRendererBarrier() override {
    OverrideMediaServiceForTesting(nullptr);
    ResumeMediaServiceOnUI();
    remote_.reset();
    receiver_.reset();
    renderer_.reset();
    media_task_runner_->ClearPendingTasks();
  }

  // media::mojom::MediaService implementation:
  void CreateInterfaceFactory(
      mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
      mojo::PendingRemote<media::mojom::FrameInterfaceFactory> frame_interfaces)
      override {}

  void FlushAndSuspendActiveRenderers(
      FlushAndSuspendActiveRenderersCallback callback) override {
    conceal_registry_.FlushAndSuspendActiveRenderers(std::move(callback));
  }

  void ResumeActiveRenderers() override {
    conceal_registry_.ResumeActiveRenderers();
  }

  bool suspend_called() const { return media_task_runner_->HasPendingTask(); }

  bool IsConcealed() {
    std::unique_ptr<media::StarboardRenderer> probe = CreateRenderer();
    testing::NiceMock<media::MockMediaResource> media_resource;
    testing::NiceMock<media::MockRendererClient> renderer_client;
    media::PipelineStatus status = media::PIPELINE_OK;
    probe->Initialize(&media_resource, &renderer_client,
                      base::BindOnce([](media::PipelineStatus* out,
                                        media::PipelineStatus s) { *out = s; },
                                     &status));
    return status == media::PIPELINE_ERROR_ABORT;
  }

  void CompleteSuspend() { media_task_runner_->RunPendingTasks(); }

  base::OnceClosure TakePendingCallback() {
    EXPECT_EQ(media_task_runner_->NumPendingTasks(), 1u);
    auto tasks = media_task_runner_->TakePendingTasks();
    if (tasks.empty()) {
      return {};
    }
    return std::move(tasks.back().task);
  }

 private:
  std::unique_ptr<media::StarboardRenderer> CreateRenderer() {
    return std::make_unique<media::StarboardRenderer>(
        media_task_runner_, std::make_unique<media::NullMediaLog>(),
        /*overlay_plane_id=*/base::UnguessableToken::Create(),
        /*audio_write_duration_local=*/base::Seconds(1),
        /*audio_write_duration_remote=*/base::Seconds(1),
        /*max_video_capabilities=*/"",
        /*max_video_resolution=*/"",
        media::StarboardRendererConfig::ExperimentalFeatures{},
        /*viewport_size=*/gfx::Size(),
#if BUILDFLAG(IS_ANDROID)
        /*android_overlay_factory_cb=*/media::AndroidOverlayMojoFactoryCB(),
#endif  // BUILDFLAG(IS_ANDROID)
        &conceal_registry_);
  }

  scoped_refptr<base::TestSimpleTaskRunner> media_task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  media::StarboardRendererConcealRegistry conceal_registry_;
  std::unique_ptr<media::StarboardRenderer> renderer_;
  mojo::Remote<media::mojom::MediaService> remote_;
  mojo::Receiver<media::mojom::MediaService> receiver_{
      this, remote_.BindNewPipeAndPassReceiver()};
};

}  // namespace

class LifecycleTest : public ShellTestBase {
 public:
  LifecycleTest() = default;

  void CreateTestShell(bool is_visible) {
    InitializeShell(is_visible);
    WebContents::CreateParams create_params(browser_context());
    create_params.desired_renderer_state =
        WebContents::CreateParams::kNoRendererProcess;
    create_params.initially_hidden = !is_visible;
    std::unique_ptr<WebContents> web_contents(
        TestWebContents::Create(create_params));

    if (is_visible) {
      EXPECT_CALL(*platform_, CreatePlatformWindow(_, _));
    }
    EXPECT_CALL(*platform_, SetContents(_));

    shell_ = new Shell(std::move(web_contents), nullptr /* splash_contents */,
                       /*should_set_delegate=*/true, /*topic=*/"",
                       /*skip_for_testing=*/true);

    if (is_visible) {
      platform_->CreatePlatformWindow(shell_, gfx::Size(1920, 1080));
    }

    Shell::FinishShellInitialization(shell_);
  }

  void TearDown() override {
    OverrideMediaServiceForTesting(nullptr);
    ResumeMediaServiceOnUI();
    if (shell_) {
      EXPECT_CALL(*platform_, DestroyShell(shell_));
      EXPECT_CALL(*platform_, CleanUp(shell_));
      shell_->Close();
      task_environment()->RunUntilIdle();
      shell_ = nullptr;
    }
    ShellTestBase::TearDown();
  }

  Shell* shell_ = nullptr;
};

TEST_F(LifecycleTest, StartupVisible) {
  CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);
}

TEST_F(LifecycleTest, StartupHidden) {
  CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::HIDDEN);
}

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_STARBOARD)
TEST_F(LifecycleTest, ClosingActivityWindowPreservesPlatformForRecreation) {
  CreateTestShell(true);
  ShellPlatformDelegate* platform = Shell::GetPlatform();
  EXPECT_CALL(*platform_, DestroyShell(shell_));
  EXPECT_CALL(*platform_, CleanUp(shell_));
  EXPECT_CALL(*platform_, DidCloseLastWindow()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::DidCloseLastWindow();
  });

  shell_->Close();
  shell_ = nullptr;

  EXPECT_TRUE(Shell::windows().empty());
  EXPECT_EQ(platform, Shell::GetPlatform());
}
#endif

TEST_F(LifecycleTest, Reveal) {
  CreateTestShell(false /* is_visible */);
  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::HIDDEN);

  // Simulate that the frame was visible before conceal, so OnReveal will
  // trigger WasShown().
  platform_->AddPreviouslyVisibleWebContentsForTesting(shell_->web_contents());

  // Trigger reveal.
  EXPECT_CALL(*platform_, OnReveal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_))
      .WillOnce(
          [](content::Shell* shell) { shell->web_contents()->WasShown(); });
  Shell::OnReveal();

  EXPECT_TRUE(platform_->IsVisible());
  // Visibility should now be VISIBLE because we called WasShown() in OnReveal
  // to unblock the renderer.
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);

  // Simulate Reveal ACK.
  platform_->OnPageVisibilityVisible(shell_);

  // Now visibility should be VISIBLE.
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);
}

TEST_F(LifecycleTest, RedundantReveal) {
  CreateTestShell(false /* is_visible */);

  // First reveal.
  EXPECT_CALL(*platform_, OnReveal()).Times(1).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_));
  Shell::OnReveal();

  // Second reveal (should be ignored by idempotent Shell logic).
  EXPECT_CALL(*platform_, OnReveal()).Times(0);
  Shell::OnReveal();
}

TEST_F(LifecycleTest, Conceal) {
  CreateTestShell(true /* is_visible */);
  EXPECT_TRUE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::VISIBLE);

  // Trigger conceal.
  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  EXPECT_CALL(*platform_, ConcealShell(shell_));
  Shell::OnConceal();

  // Symmetrical unit-test trigger: since there are no active Blink frames to
  // dispatch Mojo visibility ACKs, we manually invoke the observer callback
  // on the platform manager base class.
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesConcealed(shell_->web_contents());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(platform_->IsVisible());
  EXPECT_EQ(shell_->web_contents()->GetVisibility(), Visibility::HIDDEN);
}

TEST_F(LifecycleTest, FreezeUnfreeze) {
  CreateTestShell(false /* is_visible */);
  TestWebContents* test_web_contents =
      static_cast<TestWebContents*>(shell_->web_contents());

  // Trigger freeze.
  EXPECT_CALL(*platform_, OnFreeze()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnFreeze();
  });
  Shell::OnFreeze();
  EXPECT_TRUE(test_web_contents->IsPageFrozen());

  // Trigger unfreeze.
  EXPECT_CALL(*platform_, OnUnfreeze()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnUnfreeze();
  });
  Shell::OnUnfreeze();
  EXPECT_FALSE(test_web_contents->IsPageFrozen());
}

TEST_F(LifecycleTest, BlurFocus) {
  CreateTestShell(true /* is_visible */);

  // Trigger blur.
  EXPECT_CALL(*platform_, OnBlur());
  Shell::OnBlur();

  // Trigger focus.
  EXPECT_CALL(*platform_, OnFocus());
  Shell::OnFocus();
}

TEST_F(LifecycleTest, ConcealSequencesConcealShellBeforeOnConcealCompleted) {
  CreateTestShell(true /* is_visible */);

  class MockLifecycleObserver : public cobalt::CobaltLifecycleManagerObserver {
   public:
    MOCK_METHOD(void,
                OnAllFramesVisible,
                (content::WebContents * web_contents),
                (override));
    MOCK_METHOD(void,
                OnConcealCompleted,
                (content::WebContents * web_contents),
                (override));
  };

  testing::StrictMock<MockLifecycleObserver> observer;
  cobalt::CobaltLifecycleManager::GetInstance()->AddObserver(&observer);

  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  EXPECT_CALL(*platform_, ConcealShell(shell_)).Times(0);
  Shell::OnConceal();
  testing::Mock::VerifyAndClearExpectations(platform_);

  testing::InSequence seq;
  EXPECT_CALL(*platform_, ConcealShell(shell_));
  EXPECT_CALL(observer, OnConcealCompleted(shell_->web_contents()));

  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesConcealed(shell_->web_contents());
  base::RunLoop().RunUntilIdle();

  cobalt::CobaltLifecycleManager::GetInstance()->RemoveObserver(&observer);
}

TEST_F(LifecycleTest, ConcealWaitsForMediaServiceBarrierBeforeConcealShell) {
  EXPECT_TRUE(content::kIsBackgroundMediaSuspendEnabled);
  CreateTestShell(true /* is_visible */);

  FakeStarboardRendererBarrier fake_barrier;

  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  Shell::OnConceal();

  // Before the StarboardRenderer barrier replies, ConcealShell must not be
  // called.
  EXPECT_CALL(*platform_, ConcealShell(shell_)).Times(0);
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesConcealed(shell_->web_contents());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(fake_barrier.suspend_called());
  EXPECT_TRUE(fake_barrier.IsConcealed());
  testing::Mock::VerifyAndClearExpectations(platform_);

  // Once the StarboardRenderer barrier replies, ConcealShell runs.
  EXPECT_CALL(*platform_, ConcealShell(shell_)).Times(1);
  fake_barrier.CompleteSuspend();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(platform_->IsVisible());

  // OnReveal clears the StarboardRenderer concealed state.
  EXPECT_CALL(*platform_, OnReveal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_));
  Shell::OnReveal();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(fake_barrier.IsConcealed());
}

TEST_F(LifecycleTest, RapidRevealAndConcealIgnoresStaleMediaBarrierCallback) {
  CreateTestShell(true /* is_visible */);

  FakeStarboardRendererBarrier fake_barrier;

  // First Conceal: enter media barrier and capture its pending callback.
  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  Shell::OnConceal();
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesConcealed(shell_->web_contents());
  task_environment()->RunUntilIdle();
  base::OnceClosure stale_barrier_cb = fake_barrier.TakePendingCallback();
  ASSERT_FALSE(stale_barrier_cb.is_null());

  // Rapid Reveal before the first media barrier finishes.
  EXPECT_CALL(*platform_, OnReveal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_));
  Shell::OnReveal();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(platform_->IsVisible());

  // Second Conceal (while web_contents is still hidden before reveal
  // completes): OnConceal() enters the media barrier directly via the
  // pending_conceal_web_contents_.empty() path.
  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  Shell::OnConceal();
  task_environment()->RunUntilIdle();
  base::OnceClosure active_barrier_cb = fake_barrier.TakePendingCallback();
  ASSERT_FALSE(active_barrier_cb.is_null());
  testing::Mock::VerifyAndClearExpectations(platform_);

  // Running the stale callback from the first conceal must be ignored and must
  // not trigger ConcealShell prematurely while the second barrier is pending.
  EXPECT_CALL(*platform_, ConcealShell(shell_)).Times(0);
  std::move(stale_barrier_cb).Run();
  task_environment()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(platform_);

  // Running the active callback from the second conceal completes ConcealShell.
  EXPECT_CALL(*platform_, ConcealShell(shell_)).Times(1);
  std::move(active_barrier_cb).Run();
  task_environment()->RunUntilIdle();
}

TEST_F(LifecycleTest,
       MediaServiceLazilyBoundWhileConcealedInheritsConcealedState) {
  CreateTestShell(true /* is_visible */);

  // Conceal before any MediaService is bound. ConcealShell completes
  // immediately without waiting for an unbound service.
  EXPECT_CALL(*platform_, OnConceal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnConceal();
  });
  Shell::OnConceal();
  EXPECT_CALL(*platform_, ConcealShell(shell_)).Times(1);
  static_cast<cobalt::CobaltLifecycleManagerObserver*>(platform_)
      ->OnAllFramesConcealed(shell_->web_contents());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(platform_->IsVisible());

  // Binding a MediaService while concealed propagates the concealed state.
  FakeStarboardRendererBarrier fake_barrier;
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(fake_barrier.suspend_called());
  EXPECT_TRUE(fake_barrier.IsConcealed());
  fake_barrier.CompleteSuspend();
  task_environment()->RunUntilIdle();

  // Revealing clears the concealed state on the now-bound MediaService.
  EXPECT_CALL(*platform_, OnReveal()).WillOnce([this]() {
    platform_->ShellPlatformDelegate::OnReveal();
  });
  EXPECT_CALL(*platform_, RevealShell(shell_));
  Shell::OnReveal();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(fake_barrier.IsConcealed());
}

}  // namespace content

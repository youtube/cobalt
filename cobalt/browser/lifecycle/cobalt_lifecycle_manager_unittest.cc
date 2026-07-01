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

#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"

#include "cobalt/browser/h5vcc_runtime/h5vcc_runtime_impl.h"
#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class MockCobaltLifecycleObserver : public CobaltLifecycleManagerObserver {
 public:
  MOCK_METHOD(void, OnAllFramesVisible, (content::WebContents*), (override));
  MOCK_METHOD(void,
              OnStartWaitingForReveal,
              (content::WebContents*),
              (override));
};

class CobaltLifecycleManagerTest : public content::ShellTestBase {
 protected:
  void SetUp() override {
    content::ShellTestBase::SetUp();
    InitializeShell(true /* is_visible */);
    manager_ = CobaltLifecycleManager::GetInstance();
  }

  std::unique_ptr<content::WebContents> CreateScopedTestWebContents() {
    content::WebContents::CreateParams create_params(browser_context());
    return std::unique_ptr<content::WebContents>(
        content::TestWebContents::Create(create_params));
  }

  content::RenderFrameHost* AddChildFrame(content::RenderFrameHost* rfh) {
    content::RenderFrameHost* child_rfh =
        content::RenderFrameHostTester::For(rfh)->AppendChild("");
    content::RenderFrameHostTester::For(child_rfh)
        ->InitializeRenderFrameIfNeeded();
    return child_rfh;
  }

  CobaltLifecycleManager* manager_;
};

TEST_F(CobaltLifecycleManagerTest, ScopedWaiting) {
  std::unique_ptr<content::WebContents> contents1 =
      CreateScopedTestWebContents();
  content::RenderFrameHostTester::For(contents1->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();

  std::unique_ptr<content::WebContents> contents2 =
      CreateScopedTestWebContents();
  content::RenderFrameHostTester::For(contents2->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();

  mojo::Remote<cobalt::mojom::CobaltLifecycleObserver> remote1;
  manager_->BindReceiver(contents1->GetPrimaryMainFrame(),
                         remote1.BindNewPipeAndPassReceiver());
  remote1->OnFrameReady();

  mojo::Remote<cobalt::mojom::CobaltLifecycleObserver> remote2;
  manager_->BindReceiver(contents2->GetPrimaryMainFrame(),
                         remote2.BindNewPipeAndPassReceiver());
  remote2->OnFrameReady();

  // Establish Mojo connections and register frames.
  base::RunLoop().RunUntilIdle();

  MockCobaltLifecycleObserver observer;
  manager_->AddObserver(&observer);

  // Start waiting for contents1.
  manager_->StartWaitingForAck(contents1.get(), PendingAck::kReveal);

  // We expect OnAllFramesVisible to be called ONLY for contents1.
  EXPECT_CALL(observer, OnAllFramesVisible(contents1.get())).Times(1);
  EXPECT_CALL(observer, OnAllFramesVisible(contents2.get())).Times(0);

  // Frame 1 reports visible via Mojo.
  remote1->OnPageVisibilityChanged(true);

  // Wait for Mojo message.
  base::RunLoop().RunUntilIdle();

  // Start waiting for contents2.
  manager_->StartWaitingForAck(contents2.get(), PendingAck::kReveal);

  EXPECT_CALL(observer, OnAllFramesVisible(contents2.get())).Times(1);

  // Frame 2 reports visible.
  remote2->OnPageVisibilityChanged(true);

  base::RunLoop().RunUntilIdle();
  manager_->RemoveObserver(&observer);

  remote1.reset();
  remote2.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(CobaltLifecycleManagerTest, ObserverNotificationIsDeferred) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();
  content::RenderFrameHostTester::For(contents->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();

  mojo::Remote<cobalt::mojom::CobaltLifecycleObserver> remote;
  manager_->BindReceiver(contents->GetPrimaryMainFrame(),
                         remote.BindNewPipeAndPassReceiver());
  remote->OnFrameReady();
  base::RunLoop().RunUntilIdle();

  MockCobaltLifecycleObserver observer;
  manager_->AddObserver(&observer);

  // Verify that notification is NOT synchronous.
  EXPECT_CALL(observer, OnStartWaitingForReveal(contents.get())).Times(0);
  manager_->StartWaitingForAck(contents.get(), PendingAck::kReveal);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify that notification is delivered asynchronously.
  EXPECT_CALL(observer, OnStartWaitingForReveal(contents.get())).Times(1);
  base::RunLoop().RunUntilIdle();
  manager_->RemoveObserver(&observer);

  remote.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(CobaltLifecycleManagerTest, MainFrameUnregistrationWhileWaiting) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();
  content::RenderFrameHostTester::For(contents->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();

  mojo::Remote<cobalt::mojom::CobaltLifecycleObserver> remote;
  manager_->BindReceiver(contents->GetPrimaryMainFrame(),
                         remote.BindNewPipeAndPassReceiver());
  remote->OnFrameReady();
  base::RunLoop().RunUntilIdle();

  MockCobaltLifecycleObserver observer;
  manager_->AddObserver(&observer);

  manager_->StartWaitingForAck(contents.get(), PendingAck::kReveal);

  // Expect OnAllFramesVisible to be called when MAIN frame is UNREGISTERED.
  EXPECT_CALL(observer, OnAllFramesVisible(contents.get())).Times(1);

  // Resetting the remote destroys the receiver, which unregisters the frame.
  remote.reset();
  base::RunLoop().RunUntilIdle();

  manager_->RemoveObserver(&observer);
}

TEST_F(CobaltLifecycleManagerTest, ImmediateCompletion) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();

  MockCobaltLifecycleObserver observer;
  manager_->AddObserver(&observer);

  // Expect immediate call.
  EXPECT_CALL(observer, OnAllFramesVisible(contents.get())).Times(1);

  manager_->StartWaitingForAck(contents.get(), PendingAck::kReveal);
  base::RunLoop().RunUntilIdle();

  manager_->RemoveObserver(&observer);
}

TEST_F(CobaltLifecycleManagerTest, DISABLED_RevealTimeout) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();

  MockCobaltLifecycleObserver observer;
  manager_->AddObserver(&observer);

  manager_->StartWaitingForAck(contents.get(), PendingAck::kReveal);

  // Expect OnAllFramesVisible to be called when TIMEOUT fires.
  EXPECT_CALL(observer, OnAllFramesVisible(contents.get())).Times(1);

  // Fast forward time by 2 seconds.
  task_environment()->FastForwardBy(base::Seconds(2));

  manager_->RemoveObserver(&observer);
}

TEST_F(CobaltLifecycleManagerTest,
       StartWaitingForUnfreezeWithDeadFrameDoesNotCrash) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();
  // Do NOT call InitializeRenderFrameIfNeeded(), so IsRenderFrameLive() returns
  // false. This simulates the state of mock frames in unit tests or extremely
  // early startup, where Rebind() would crash if called.

  // Start waiting for unfreeze. This shouldn't crash.
  manager_->StartWaitingForAck(contents.get(), PendingAck::kUnfreeze);
  base::RunLoop().RunUntilIdle();
}

TEST_F(CobaltLifecycleManagerTest,
       ReceiverDisconnectDoesNotUnregisterActiveFrame) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();
  content::RenderFrameHostTester::For(contents->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();

  mojo::Remote<cobalt::mojom::CobaltLifecycleObserver> remote1;
  manager_->BindReceiver(contents->GetPrimaryMainFrame(),
                         remote1.BindNewPipeAndPassReceiver());
  remote1->OnFrameReady();
  base::RunLoop().RunUntilIdle();

  // Simulate a re-bind by creating a second remote for the SAME frame.
  mojo::Remote<cobalt::mojom::CobaltLifecycleObserver> remote2;
  manager_->BindReceiver(contents->GetPrimaryMainFrame(),
                         remote2.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  // Disconnect the FIRST remote (simulating the async disconnect of the old
  // pipe).
  remote1.reset();
  base::RunLoop().RunUntilIdle();

  // The frame should STILL be waiting/registered because remote2 is active!
  MockCobaltLifecycleObserver observer;
  manager_->AddObserver(&observer);

  manager_->StartWaitingForAck(contents.get(), PendingAck::kReveal);

  // If it was unregistered, it would trigger immediate completion (call times
  // 1). Because it's still registered, it waits (call times 0).
  EXPECT_CALL(observer, OnAllFramesVisible(contents.get())).Times(0);
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Disconnect the second remote, NOW it should unregister and complete.
  EXPECT_CALL(observer, OnAllFramesVisible(contents.get())).Times(1);
  remote2.reset();
  base::RunLoop().RunUntilIdle();

  manager_->RemoveObserver(&observer);
}

}  // namespace cobalt

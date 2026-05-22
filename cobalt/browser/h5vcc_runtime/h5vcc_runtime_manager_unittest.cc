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

#include "cobalt/browser/h5vcc_runtime/h5vcc_runtime_manager.h"

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

namespace h5vcc_runtime {

class MockH5vccRuntimeObserver : public H5vccRuntimeObserver {
 public:
  MOCK_METHOD(void, OnAllFramesVisible, (content::WebContents*), (override));
  MOCK_METHOD(void,
              OnStartWaitingForReveal,
              (content::WebContents*),
              (override));
};

class H5vccRuntimeManagerTest : public content::ShellTestBase {
 protected:
  void SetUp() override {
    content::ShellTestBase::SetUp();
    InitializeShell(true /* is_visible */);
    manager_ = H5vccRuntimeManager::GetInstance();
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

  H5vccRuntimeManager* manager_;
};

TEST_F(H5vccRuntimeManagerTest, ScopedWaiting) {
  std::unique_ptr<content::WebContents> contents1 =
      CreateScopedTestWebContents();
  std::unique_ptr<content::WebContents> contents2 =
      CreateScopedTestWebContents();

  mojo::Remote<mojom::H5vccRuntime> remote1;
  H5vccRuntimeImpl::Create(contents1->GetPrimaryMainFrame(),
                           remote1.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::H5vccRuntime> remote2;
  H5vccRuntimeImpl::Create(contents2->GetPrimaryMainFrame(),
                           remote2.BindNewPipeAndPassReceiver());

  MockH5vccRuntimeObserver observer;
  manager_->AddObserver(&observer);

  // Start waiting for contents1.
  manager_->StartWaitingForReveal(contents1.get());

  // We expect OnAllFramesVisible to be called ONLY for contents1.
  EXPECT_CALL(observer, OnAllFramesVisible(contents1.get())).Times(1);
  EXPECT_CALL(observer, OnAllFramesVisible(contents2.get())).Times(0);

  // Frame 1 reports visible via Mojo.
  remote1->PageVisibilityChanged();

  // Wait for Mojo message.
  base::RunLoop().RunUntilIdle();

  // Start waiting for contents2.
  manager_->StartWaitingForReveal(contents2.get());

  EXPECT_CALL(observer, OnAllFramesVisible(contents2.get())).Times(1);

  // Frame 2 reports visible.
  remote2->PageVisibilityChanged();

  base::RunLoop().RunUntilIdle();

  manager_->RemoveObserver(&observer);
}

TEST_F(H5vccRuntimeManagerTest, ObserverNotificationIsDeferred) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();

  MockH5vccRuntimeObserver observer;
  manager_->AddObserver(&observer);

  // Verify that notification is NOT synchronous.
  EXPECT_CALL(observer, OnStartWaitingForReveal(contents.get())).Times(0);
  manager_->StartWaitingForReveal(contents.get());
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify that notification is delivered after running tasks.
  EXPECT_CALL(observer, OnStartWaitingForReveal(contents.get())).Times(1);
  base::RunLoop().RunUntilIdle();

  manager_->RemoveObserver(&observer);
}

TEST_F(H5vccRuntimeManagerTest, MainFrameUnregistrationWhileWaiting) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();

  mojo::Remote<mojom::H5vccRuntime> remote;
  H5vccRuntimeImpl::Create(contents->GetPrimaryMainFrame(),
                           remote.BindNewPipeAndPassReceiver());

  MockH5vccRuntimeObserver observer;
  manager_->AddObserver(&observer);

  manager_->StartWaitingForReveal(contents.get());

  // Expect OnAllFramesVisible to be called when MAIN frame is UNREGISTERED.
  EXPECT_CALL(observer, OnAllFramesVisible(contents.get())).Times(1);

  // Resetting the remote destroys the DocumentService, which unregisters the
  // frame.
  remote.reset();
  base::RunLoop().RunUntilIdle();

  manager_->RemoveObserver(&observer);
}

TEST_F(H5vccRuntimeManagerTest, ImmediateCompletion) {
  std::unique_ptr<content::WebContents> contents =
      CreateScopedTestWebContents();

  MockH5vccRuntimeObserver observer;
  manager_->AddObserver(&observer);

  // Expect immediate call.
  EXPECT_CALL(observer, OnAllFramesVisible(contents.get())).Times(1);

  manager_->StartWaitingForReveal(contents.get());

  manager_->RemoveObserver(&observer);
}

}  // namespace h5vcc_runtime

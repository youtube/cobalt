// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/browser_process_task_provider.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/grit/generated_resources.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

class BrowserProcessTaskProviderTest
    : public testing::Test,
      public TaskProviderObserver {
 public:
  BrowserProcessTaskProviderTest()
      : provided_task_(nullptr) {
  }

  BrowserProcessTaskProviderTest(const BrowserProcessTaskProviderTest&) =
      delete;
  BrowserProcessTaskProviderTest& operator=(
      const BrowserProcessTaskProviderTest&) = delete;
  ~BrowserProcessTaskProviderTest() override = default;

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override {
    provided_task_ = task;
  }
  void TaskRemoved(Task* task) override {
    // This will never be called in the case of a browser process task provider.
    FAIL();
  }

 protected:
  raw_ptr<Task> provided_task_;
};

// Tests the browser process task provider and browser process task itself.
TEST_F(BrowserProcessTaskProviderTest, TestObserving) {
  BrowserProcessTaskProvider provider;
  EXPECT_EQ(nullptr, provided_task_.get());
  provider.SetObserver(this);
  EXPECT_NE(nullptr, provided_task_);
  provider.ClearObserver();
  EXPECT_NE(nullptr, provided_task_);
}

// Testing retrieving the task from the provider using the ids of a URL request.
TEST_F(BrowserProcessTaskProviderTest, GetTaskOfUrlRequest) {
  BrowserProcessTaskProvider provider;
  EXPECT_EQ(nullptr, provided_task_.get());
  provider.SetObserver(this);
  EXPECT_NE(nullptr, provided_task_);

  Task* result = provider.GetTaskOfUrlRequest(2, 0);
  EXPECT_EQ(nullptr, result);
  result = provider.GetTaskOfUrlRequest(network::mojom::kBrowserProcessId, 0);
  EXPECT_EQ(provided_task_, result);
}

// Test the provided browser process task itself.
TEST_F(BrowserProcessTaskProviderTest, TestProvidedTask) {
  BrowserProcessTaskProvider provider;
  EXPECT_EQ(nullptr, provided_task_.get());
  provider.SetObserver(this);
  ASSERT_NE(nullptr, provided_task_);

  EXPECT_EQ(base::GetCurrentProcessHandle(), provided_task_->process_handle());
  EXPECT_EQ(base::GetCurrentProcId(), provided_task_->process_id());
  EXPECT_FALSE(provided_task_->ReportsWebCacheStats());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_WEB_BROWSER_CELL_TEXT),
            provided_task_->title());
  EXPECT_EQ(Task::BROWSER, provided_task_->GetType());
  EXPECT_EQ(0, provided_task_->GetChildProcessUniqueID());
  const int received_bytes = 1024;
  EXPECT_EQ(0, provided_task_->GetNetworkUsageRate());
  provided_task_->OnNetworkBytesRead(received_bytes);
  // Do a refresh with a 1-second update time.
  provided_task_->Refresh(base::Seconds(1), REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_EQ(received_bytes, provided_task_->GetNetworkUsageRate());
}

}  // namespace task_manager

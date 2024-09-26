// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/swap_promise_manager.h"

#include <memory>
#include <utility>

#include "base/time/time.h"
#include "cc/test/mock_latency_info_swap_promise_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace cc {
namespace {

class MockSwapPromise : public SwapPromise {
 public:
  MockSwapPromise() = default;
  ~MockSwapPromise() override = default;

  void DidActivate() override {}
  void WillSwap(viz::CompositorFrameMetadata* metadata) override {}
  void DidSwap() override {}
  DidNotSwapAction DidNotSwap(DidNotSwapReason reason,
                              base::TimeTicks ts) override {
    return DidNotSwapAction::BREAK_PROMISE;
  }
  MOCK_METHOD0(OnCommit, void());
  int64_t GetTraceId() const override { return 0; }
};

TEST(SwapPromiseManagerTest, SwapPromiseMonitors) {
  SwapPromiseManager manager;
  StrictMock<MockLatencyInfoSwapPromiseMonitor> monitor(&manager);

  EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(2);

  manager.NotifyLatencyInfoSwapPromiseMonitors();
  manager.NotifyLatencyInfoSwapPromiseMonitors();
}

TEST(SwapPromiseManagerTest, SwapPromises) {
  SwapPromiseManager manager;
  std::unique_ptr<StrictMock<MockSwapPromise>> swap_promise =
      std::make_unique<StrictMock<MockSwapPromise>>();
  MockSwapPromise* mock_promise = swap_promise.get();

  manager.QueueSwapPromise(std::move(swap_promise));

  EXPECT_CALL(*mock_promise, OnCommit()).Times(1);
  manager.WillCommit();

  std::vector<std::unique_ptr<SwapPromise>> swap_promise_list =
      manager.TakeSwapPromises();
  // Now that we've taken the promises, this shouldn't trigger a call.
  manager.WillCommit();

  EXPECT_EQ(1, static_cast<int>(swap_promise_list.size()));
}

}  // namespace
}  // namespace cc

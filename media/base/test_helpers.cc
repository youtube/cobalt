// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_helpers.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "base/timer.h"
#include "media/base/bind_to_loop.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

// Utility mock for testing methods expecting Closures and PipelineStatusCBs.
class MockCallback : public base::RefCountedThreadSafe<MockCallback> {
 public:
  MockCallback();
  MOCK_METHOD0(Run, void());
  MOCK_METHOD1(RunWithStatus, void(PipelineStatus));

 protected:
  friend class base::RefCountedThreadSafe<MockCallback>;
  virtual ~MockCallback();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

MockCallback::MockCallback() {}
MockCallback::~MockCallback() {}

base::Closure NewExpectedClosure() {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, Run());
  return base::Bind(&MockCallback::Run, callback);
}

PipelineStatusCB NewExpectedStatusCB(PipelineStatus status) {
  StrictMock<MockCallback>* callback = new StrictMock<MockCallback>();
  EXPECT_CALL(*callback, RunWithStatus(status));
  return base::Bind(&MockCallback::RunWithStatus, callback);
}

WaitableMessageLoopEvent::WaitableMessageLoopEvent()
    : message_loop_(MessageLoop::current()),
      signaled_(false),
      status_(PIPELINE_OK) {
  DCHECK(message_loop_);
}

WaitableMessageLoopEvent::~WaitableMessageLoopEvent() {}

base::Closure WaitableMessageLoopEvent::GetClosure() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  return BindToLoop(message_loop_->message_loop_proxy(), base::Bind(
      &WaitableMessageLoopEvent::OnCallback, base::Unretained(this),
      PIPELINE_OK));
}

PipelineStatusCB WaitableMessageLoopEvent::GetPipelineStatusCB() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  return BindToLoop(message_loop_->message_loop_proxy(), base::Bind(
      &WaitableMessageLoopEvent::OnCallback, base::Unretained(this)));
}

void WaitableMessageLoopEvent::RunAndWait() {
  RunAndWaitForStatus(PIPELINE_OK);
}

void WaitableMessageLoopEvent::RunAndWaitForStatus(PipelineStatus expected) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  base::Timer timer(false, false);
  timer.Start(FROM_HERE, TestTimeouts::action_timeout(), base::Bind(
      &WaitableMessageLoopEvent::OnTimeout, base::Unretained(this)));

  DCHECK(!signaled_) << "Already signaled";
  message_loop_->Run();
  EXPECT_TRUE(signaled_);
  EXPECT_EQ(expected, status_);
}

void WaitableMessageLoopEvent::OnCallback(PipelineStatus status) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  signaled_ = true;
  status_ = status;
  message_loop_->QuitWhenIdle();
}

void WaitableMessageLoopEvent::OnTimeout() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  ADD_FAILURE() << "Timed out waiting for message loop to quit";
  message_loop_->QuitWhenIdle();
}

}  // namespace media

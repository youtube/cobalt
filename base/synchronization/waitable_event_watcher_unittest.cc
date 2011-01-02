// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class QuitDelegate : public WaitableEventWatcher::Delegate {
 public:
  virtual void OnWaitableEventSignaled(WaitableEvent* event) {
    MessageLoop::current()->Quit();
  }
};

class DecrementCountDelegate : public WaitableEventWatcher::Delegate {
 public:
  explicit DecrementCountDelegate(int* counter) : counter_(counter) {
  }
  virtual void OnWaitableEventSignaled(WaitableEvent* object) {
    --(*counter_);
  }
 private:
  int* counter_;
};

void RunTest_BasicSignal(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(true, false);

  WaitableEventWatcher watcher;
  EXPECT_TRUE(watcher.GetWatchedEvent() == NULL);

  QuitDelegate delegate;
  watcher.StartWatching(&event, &delegate);
  EXPECT_EQ(&event, watcher.GetWatchedEvent());

  event.Signal();

  MessageLoop::current()->Run();

  EXPECT_TRUE(watcher.GetWatchedEvent() == NULL);
}

void RunTest_BasicCancel(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(true, false);

  WaitableEventWatcher watcher;

  QuitDelegate delegate;
  watcher.StartWatching(&event, &delegate);

  watcher.StopWatching();
}

void RunTest_CancelAfterSet(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(true, false);

  WaitableEventWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);

  watcher.StartWatching(&event, &delegate);

  event.Signal();

  // Let the background thread do its business
  base::PlatformThread::Sleep(30);

  watcher.StopWatching();

  MessageLoop::current()->RunAllPending();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);
}

void RunTest_OutlivesMessageLoop(MessageLoop::Type message_loop_type) {
  // Simulate a MessageLoop that dies before an WaitableEventWatcher.  This
  // ordinarily doesn't happen when people use the Thread class, but it can
  // happen when people use the Singleton pattern or atexit.
  WaitableEvent event(true, false);
  {
    WaitableEventWatcher watcher;
    {
      MessageLoop message_loop(message_loop_type);

      QuitDelegate delegate;
      watcher.StartWatching(&event, &delegate);
    }
  }
}

void RunTest_DeleteUnder(MessageLoop::Type message_loop_type) {
  // Delete the WaitableEvent out from under the Watcher. This is explictly
  // allowed by the interface.

  MessageLoop message_loop(message_loop_type);

  {
    WaitableEventWatcher watcher;

    WaitableEvent* event = new WaitableEvent(false, false);
    QuitDelegate delegate;
    watcher.StartWatching(event, &delegate);
    delete event;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TEST(WaitableEventWatcherTest, BasicSignal) {
  RunTest_BasicSignal(MessageLoop::TYPE_DEFAULT);
  RunTest_BasicSignal(MessageLoop::TYPE_IO);
  RunTest_BasicSignal(MessageLoop::TYPE_UI);
}

TEST(WaitableEventWatcherTest, BasicCancel) {
  RunTest_BasicCancel(MessageLoop::TYPE_DEFAULT);
  RunTest_BasicCancel(MessageLoop::TYPE_IO);
  RunTest_BasicCancel(MessageLoop::TYPE_UI);
}

TEST(WaitableEventWatcherTest, CancelAfterSet) {
  RunTest_CancelAfterSet(MessageLoop::TYPE_DEFAULT);
  RunTest_CancelAfterSet(MessageLoop::TYPE_IO);
  RunTest_CancelAfterSet(MessageLoop::TYPE_UI);
}

TEST(WaitableEventWatcherTest, OutlivesMessageLoop) {
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_DEFAULT);
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_IO);
  RunTest_OutlivesMessageLoop(MessageLoop::TYPE_UI);
}

#if defined(OS_WIN)
// Crashes sometimes on vista.  http://crbug.com/62119
#define MAYBE_DeleteUnder DISABLED_DeleteUnder
#else
#define MAYBE_DeleteUnder DeleteUnder
#endif
TEST(WaitableEventWatcherTest, MAYBE_DeleteUnder) {
  RunTest_DeleteUnder(MessageLoop::TYPE_DEFAULT);
  RunTest_DeleteUnder(MessageLoop::TYPE_IO);
  RunTest_DeleteUnder(MessageLoop::TYPE_UI);
}

}  // namespace base

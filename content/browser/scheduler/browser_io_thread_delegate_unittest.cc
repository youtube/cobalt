// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_io_thread_delegate.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(BrowserIOThreadDelegateTest, CanPostTasksToThread) {
  base::Thread thread("my_thread");

  auto delegate = std::make_unique<BrowserIOThreadDelegate>();
  auto handle = delegate->GetHandle();
  handle->OnStartupComplete();

  base::Thread::Options options;
  options.delegate = std::move(delegate);
  thread.StartWithOptions(std::move(options));

  auto runner =
      handle->GetBrowserTaskRunner(BrowserTaskQueues::QueueType::kUserBlocking);

  base::WaitableEvent event;
  runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&event)));
  event.Wait();
}

TEST(BrowserIOThreadDelegateTest, DefaultTaskRunnerIsAlwaysActive) {
  base::Thread thread("my_thread");

  auto delegate = std::make_unique<BrowserIOThreadDelegate>();
  auto task_runner = delegate->GetDefaultTaskRunner();

  base::Thread::Options options;
  options.delegate = std::move(delegate);
  thread.StartWithOptions(std::move(options));

  base::WaitableEvent event;
  task_runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&event)));
  event.Wait();
}

}  // namespace
}  // namespace content

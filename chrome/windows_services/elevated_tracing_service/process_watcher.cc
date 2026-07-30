// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/process_watcher.h"

#include <windows.h>

#include <array>
#include <functional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"

namespace elevated_tracing_service {

namespace {

// Waits for either the watched process to terminate or for the shutdown event
// to be signaled. In the former case, the `on_terminated` closure is
// run before exiting. `completed_event` is signaled when the task is complete.
void WatchInThreadPool(base::Process process,
                       base::OnceClosure on_terminated,
                       HANDLE startup_event,
                       base::win::ScopedHandle shutdown_event,
                       base::win::ScopedHandle completed_event,
                       base::PlatformThreadId& watch_thread_id) {
  // Give the ID of this thread to the ProcessWatcher so that it can avoid
  // waiting on itself during destruction.
  watch_thread_id = base::PlatformThread::CurrentId();

  // Signal that the task is ready to watch.
  ::SetEvent(std::exchange(startup_event, nullptr));

  DWORD result;
  {
    base::ScopedBlockingCall will_block(FROM_HERE,
                                        base::BlockingType::WILL_BLOCK);
    HANDLE handles[] = {process.Handle(), shutdown_event.get()};
    result = ::WaitForMultipleObjects(std::size(handles), &handles[0],
                                      /*bWaitAll=*/FALSE,
                                      /*dwMilliseconds=*/INFINITE);
  }
  CHECK_NE(result, WAIT_FAILED);
  if (result == WAIT_OBJECT_0) {
    std::move(on_terminated).Run();
  }  // else the shutdown event was signaled.

  ::SetEvent(completed_event.get());
}

}  // namespace

ProcessWatcher::ProcessWatcher(base::Process process,
                               base::OnceClosure on_terminated) {
  // An event that is signaled by the watch task when it is ready to watch. No
  // need to duplicate this for the watch task, as this instance will outlive
  // the `SetEvent()` call in the task.
  base::WaitableEvent startup_event;

  // Prepare a dup of the shutdown event for the task to wait on.
  HANDLE shutdown_event = nullptr;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), shutdown_event_.handle(),
                          ::GetCurrentProcess(), &shutdown_event,
                          /*dwDesiredAccess=*/0,
                          /*bInheritHandle=*/FALSE, DUPLICATE_SAME_ACCESS));

  // Prepare a dup of the completed event for the task to signal.
  HANDLE completed_event = nullptr;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), completed_event_.handle(),
                          ::GetCurrentProcess(), &completed_event,
                          /*dwDesiredAccess=*/0,
                          /*bInheritHandle=*/FALSE, DUPLICATE_SAME_ACCESS));

  base::ThreadPool::CreateTaskRunner(
      {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&WatchInThreadPool, std::move(process),
                         std::move(on_terminated), startup_event.handle(),
                         base::win::ScopedHandle(shutdown_event),
                         base::win::ScopedHandle(completed_event),
                         std::ref(watch_thread_id_)));

  // Wait for the watch task to signal that it is ready.
  startup_event.Wait();
}

ProcessWatcher::~ProcessWatcher() {
  // Signal that the watch task should exit if it is still watching the process.
  shutdown_event_.Signal();

  // Wait for the watch task to complete before continuing with destruction,
  // unless destruction is taking place on the watch thread itself. This can
  // happen if client termination synchronously triggers destruction via COM.
  if (base::PlatformThread::CurrentId() != watch_thread_id_) {
    completed_event_.Wait();
  }
}

}  // namespace elevated_tracing_service

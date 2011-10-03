// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/worker_pool.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/tracked_objects.h"

namespace base {

namespace {

struct PendingTask {
  PendingTask(
      const tracked_objects::Location& posted_from,
      const base::Closure& task)
      : posted_from(posted_from),
        task(task) {
#if defined(TRACK_ALL_TASK_OBJECTS)
    post_births = tracked_objects::ThreadData::TallyABirthIfActive(posted_from);
    time_posted = TimeTicks::Now();
#endif  // defined(TRACK_ALL_TASK_OBJECTS)
  }

#if defined(TRACK_ALL_TASK_OBJECTS)
  // Counter for location where the Closure was posted from.
  tracked_objects::Births* post_births;

  // Time the task was posted.
  TimeTicks time_posted;
#endif  // defined(TRACK_ALL_TASK_OBJECTS)

  // The site this PendingTask was posted from.
  tracked_objects::Location posted_from;

  // The task to run.
  base::Closure task;
};

DWORD CALLBACK WorkItemCallback(void* param) {
  PendingTask* pending_task = static_cast<PendingTask*>(param);
  UNSHIPPED_TRACE_EVENT2("task", "WorkItemCallback::Run",
                         "src_file", pending_task->posted_from.file_name(),
                         "src_func", pending_task->posted_from.function_name());
  pending_task->task.Run();
#if defined(TRACK_ALL_TASK_OBJECTS)
  tracked_objects::ThreadData::TallyADeathIfActive(
      pending_task->post_births,
      TimeTicks::Now() - pending_task->time_posted);
#endif  // defined(TRACK_ALL_TASK_OBJECTS)
  delete pending_task;
  return 0;
}

// Takes ownership of |pending_task|
bool PostTaskInternal(PendingTask* pending_task, bool task_is_slow) {
  ULONG flags = 0;
  if (task_is_slow)
    flags |= WT_EXECUTELONGFUNCTION;

  if (!QueueUserWorkItem(WorkItemCallback, pending_task, flags)) {
    DLOG(ERROR) << "QueueUserWorkItem failed: " << GetLastError();
    delete pending_task;
    return false;
  }

  return true;
}

}  // namespace

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  PendingTask* pending_task =
      new PendingTask(from_here,
                      base::Bind(&subtle::TaskClosureAdapter::Run,
                                 new subtle::TaskClosureAdapter(task)));
  return PostTaskInternal(pending_task, task_is_slow);
}

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          const base::Closure& task, bool task_is_slow) {
  PendingTask* pending_task = new PendingTask(from_here, task);
  return PostTaskInternal(pending_task, task_is_slow);
}

}  // namespace base

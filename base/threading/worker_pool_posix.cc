// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/worker_pool_posix.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/threading/platform_thread.h"
#include "base/threading/worker_pool.h"

namespace base {

namespace {

const int kIdleSecondsBeforeExit = 10 * 60;
// A stack size of 64 KB is too small for the CERT_PKIXVerifyCert
// function of NSS because of NSS bug 439169.
const int kWorkerThreadStackSize = 128 * 1024;

class WorkerPoolImpl {
 public:
  WorkerPoolImpl();
  ~WorkerPoolImpl();

  void PostTask(const tracked_objects::Location& from_here, Task* task,
                bool task_is_slow);

 private:
  scoped_refptr<base::PosixDynamicThreadPool> pool_;
};

WorkerPoolImpl::WorkerPoolImpl()
    : pool_(new base::PosixDynamicThreadPool("WorkerPool",
                                             kIdleSecondsBeforeExit)) {
}

WorkerPoolImpl::~WorkerPoolImpl() {
  pool_->Terminate();
}

void WorkerPoolImpl::PostTask(const tracked_objects::Location& from_here,
                              Task* task, bool task_is_slow) {
  task->SetBirthPlace(from_here);
  pool_->PostTask(task);
}

base::LazyInstance<WorkerPoolImpl> g_lazy_worker_pool(base::LINKER_INITIALIZED);

class WorkerThread : public PlatformThread::Delegate {
 public:
  WorkerThread(const std::string& name_prefix, int idle_seconds_before_exit,
               base::PosixDynamicThreadPool* pool)
      : name_prefix_(name_prefix),
        idle_seconds_before_exit_(idle_seconds_before_exit),
        pool_(pool) {}

  virtual void ThreadMain();

 private:
  const std::string name_prefix_;
  const int idle_seconds_before_exit_;
  scoped_refptr<base::PosixDynamicThreadPool> pool_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

void WorkerThread::ThreadMain() {
  const std::string name = base::StringPrintf(
      "%s/%d", name_prefix_.c_str(), PlatformThread::CurrentId());
  PlatformThread::SetName(name.c_str());

  for (;;) {
    Task* task = pool_->WaitForTask();
    if (!task)
      break;
    task->Run();
    delete task;
  }

  // The WorkerThread is non-joinable, so it deletes itself.
  delete this;
}

}  // namespace

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  g_lazy_worker_pool.Pointer()->PostTask(from_here, task, task_is_slow);
  return true;
}

PosixDynamicThreadPool::PosixDynamicThreadPool(
    const std::string& name_prefix,
    int idle_seconds_before_exit)
    : name_prefix_(name_prefix),
      idle_seconds_before_exit_(idle_seconds_before_exit),
      tasks_available_cv_(&lock_),
      num_idle_threads_(0),
      terminated_(false),
      num_idle_threads_cv_(NULL) {}

PosixDynamicThreadPool::~PosixDynamicThreadPool() {
  while (!tasks_.empty()) {
    Task* task = tasks_.front();
    tasks_.pop();
    delete task;
  }
}

void PosixDynamicThreadPool::Terminate() {
  {
    AutoLock locked(lock_);
    DCHECK(!terminated_) << "Thread pool is already terminated.";
    terminated_ = true;
  }
  tasks_available_cv_.Broadcast();
}

void PosixDynamicThreadPool::PostTask(Task* task) {
  AutoLock locked(lock_);
  DCHECK(!terminated_) <<
      "This thread pool is already terminated.  Do not post new tasks.";

  tasks_.push(task);

  // We have enough worker threads.
  if (static_cast<size_t>(num_idle_threads_) >= tasks_.size()) {
    tasks_available_cv_.Signal();
  } else {
    // The new PlatformThread will take ownership of the WorkerThread object,
    // which will delete itself on exit.
    WorkerThread* worker =
        new WorkerThread(name_prefix_, idle_seconds_before_exit_, this);
    PlatformThread::CreateNonJoinable(kWorkerThreadStackSize, worker);
  }
}

Task* PosixDynamicThreadPool::WaitForTask() {
  AutoLock locked(lock_);

  if (terminated_)
    return NULL;

  if (tasks_.empty()) {  // No work available, wait for work.
    num_idle_threads_++;
    if (num_idle_threads_cv_.get())
      num_idle_threads_cv_->Signal();
    tasks_available_cv_.TimedWait(
        TimeDelta::FromSeconds(kIdleSecondsBeforeExit));
    num_idle_threads_--;
    if (num_idle_threads_cv_.get())
      num_idle_threads_cv_->Signal();
    if (tasks_.empty()) {
      // We waited for work, but there's still no work.  Return NULL to signal
      // the thread to terminate.
      return NULL;
    }
  }

  Task* task = tasks_.front();
  tasks_.pop();
  return task;
}

}  // namespace base

// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/in_flight_io.h"

#include "base/logging.h"

namespace disk_cache {

BackgroundIO::BackgroundIO(InFlightIO* controller)
    : controller_(controller), result_(-1), io_completed_(true, false) {
}

// Runs on the primary thread.
void BackgroundIO::OnIOSignalled() {
  if (controller_)
    controller_->InvokeCallback(this, false);
}

void BackgroundIO::Cancel() {
  DCHECK(controller_);
  controller_ = NULL;
}

BackgroundIO::~BackgroundIO() {}

// Runs on the background thread.
void BackgroundIO::NotifyController() {
  controller_->OnIOComplete(this);
}

// ---------------------------------------------------------------------------

InFlightIO::InFlightIO()
    : callback_thread_(base::MessageLoopProxy::CreateForCurrentThread()),
      running_(false), single_thread_(false) {
}

InFlightIO::~InFlightIO() {
}

void InFlightIO::WaitForPendingIO() {
  while (!io_list_.empty()) {
    // Block the current thread until all pending IO completes.
    IOList::iterator it = io_list_.begin();
    InvokeCallback(*it, true);
  }
}

// Runs on a background thread.
void InFlightIO::OnIOComplete(BackgroundIO* operation) {
#ifndef NDEBUG
  if (callback_thread_->BelongsToCurrentThread()) {
    DCHECK(single_thread_ || !running_);
    single_thread_ = true;
  }
  running_ = true;
#endif

  callback_thread_->PostTask(FROM_HERE,
                             NewRunnableMethod(operation,
                                               &BackgroundIO::OnIOSignalled));
  operation->io_completed()->Signal();
}

// Runs on the primary thread.
void InFlightIO::InvokeCallback(BackgroundIO* operation, bool cancel_task) {
  operation->io_completed()->Wait();

  if (cancel_task)
    operation->Cancel();

  // Make sure that we remove the operation from the list before invoking the
  // callback (so that a subsequent cancel does not invoke the callback again).
  DCHECK(io_list_.find(operation) != io_list_.end());
  io_list_.erase(make_scoped_refptr(operation));
  OnOperationComplete(operation, cancel_task);
}

// Runs on the primary thread.
void InFlightIO::OnOperationPosted(BackgroundIO* operation) {
  DCHECK(callback_thread_->BelongsToCurrentThread());
  io_list_.insert(make_scoped_refptr(operation));
}

}  // namespace disk_cache

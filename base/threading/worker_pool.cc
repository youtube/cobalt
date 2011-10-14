// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/worker_pool.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/threading/post_task_and_reply_impl.h"
#include "base/tracked_objects.h"

namespace base {

namespace {

class PostTaskAndReplyWorkerPool : public internal::PostTaskAndReplyImpl {
 public:
  PostTaskAndReplyWorkerPool(bool task_is_slow) : task_is_slow_(task_is_slow) {
  }

 private:
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        const Closure& task) OVERRIDE {
    return WorkerPool::PostTask(from_here, task, task_is_slow_);
  }

  bool task_is_slow_;
};

}  // namespace

bool WorkerPool::PostTaskAndReply(const tracked_objects::Location& from_here,
                                  const Closure& task,
                                  const Closure& reply,
                                  bool task_is_slow) {
  return PostTaskAndReplyWorkerPool(task_is_slow).PostTaskAndReply(
      from_here, task, reply);
}

}  // namespace base

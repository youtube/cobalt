// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_PROXY_H_
#define BASE_MESSAGE_LOOP_PROXY_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/task.h"

// This class provides a thread-safe refcounted interface to the Post* methods
// of a message loop. This class can outlive the target message loop.
class MessageLoopProxy : public base::RefCountedThreadSafe<MessageLoopProxy> {
 public:
  // These are the same methods in message_loop.h, but are guaranteed to either
  // get posted to the MessageLoop if it's still alive, or be deleted otherwise.
  // They return true iff the thread existed and the task was posted.  Note that
  // even if the task is posted, there's no guarantee that it will run, since
  // the target thread may already have a Quit message in its queue.
  virtual bool PostTask(const tracked_objects::Location& from_here,
                        Task* task) = 0;
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               Task* task, int64 delay_ms) = 0;
  virtual bool PostNonNestableTask(const tracked_objects::Location& from_here,
                                   Task* task) = 0;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      Task* task,
      int64 delay_ms) = 0;

  template <class T>
  bool DeleteSoon(const tracked_objects::Location& from_here,
                  T* object) {
    return PostNonNestableTask(from_here, new DeleteTask<T>(object));
  }
  template <class T>
  bool ReleaseSoon(const tracked_objects::Location& from_here,
                   T* object) {
    return PostNonNestableTask(from_here, new ReleaseTask<T>(object));
  }
};

#endif  // BASE_MESSAGE_LOOP_PROXY_H_


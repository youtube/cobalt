// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SERIAL_RUNNER_H_
#define MEDIA_BASE_SERIAL_RUNNER_H_

#include <queue>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/pipeline_status.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

// Runs a series of bound functions accepting Closures or PipelineStatusCB.
// SerialRunner doesn't use regular Closure/PipelineStatusCBs as it late binds
// the completion callback as the series progresses.
class SerialRunner {
 public:
  typedef base::Callback<void(const base::Closure&)> BoundClosure;
  typedef base::Callback<void(const PipelineStatusCB&)> BoundPipelineStatusCB;

  // Serial queue of bound functions to run.
  class Queue {
   public:
    Queue();
    ~Queue();

    void Push(const BoundClosure& bound_fn);
    void Push(const BoundPipelineStatusCB& bound_fn);

   private:
    friend class SerialRunner;

    BoundPipelineStatusCB Pop();
    bool empty();

    std::queue<BoundPipelineStatusCB> bound_fns_;
  };

  // Executes the bound functions in series, executing |done_cb| when finished.
  //
  // All bound functions are executed on the thread that Run() is called on,
  // including |done_cb|.
  //
  // Deleting the object will prevent execution of any unstarted bound
  // functions, including |done_cb|.
  static scoped_ptr<SerialRunner> Run(
      const Queue& bound_fns, const PipelineStatusCB& done_cb);

 private:
  friend class scoped_ptr<SerialRunner>;

  SerialRunner(const Queue& bound_fns, const PipelineStatusCB& done_cb);
  ~SerialRunner();

  void RunNextInSeries(PipelineStatus last_status);

  base::WeakPtrFactory<SerialRunner> weak_this_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  Queue bound_fns_;
  PipelineStatusCB done_cb_;

  DISALLOW_COPY_AND_ASSIGN(SerialRunner);
};

}  // namespace media

#endif  // MEDIA_BASE_SERIAL_RUNNER_H_

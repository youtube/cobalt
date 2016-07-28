// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/serial_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"

namespace media {

// Converts a bound function accepting a Closure into a bound function
// accepting a PipelineStatusCB. Since closures have no way of reporting a
// status |status_cb| is executed with PIPELINE_OK.
static void RunBoundClosure(
    const SerialRunner::BoundClosure& bound_closure,
    const PipelineStatusCB& status_cb) {
  bound_closure.Run(base::Bind(status_cb, PIPELINE_OK));
}

// Runs |status_cb| with |last_status| on |message_loop|.
static void RunOnMessageLoop(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const PipelineStatusCB& status_cb,
    PipelineStatus last_status) {
  // Force post to permit cancellation of a series in the scenario where all
  // bound functions run on the same thread.
  message_loop->PostTask(FROM_HERE, base::Bind(status_cb, last_status));
}

SerialRunner::Queue::Queue() {}
SerialRunner::Queue::~Queue() {}

void SerialRunner::Queue::Push(
    const BoundClosure& bound_closure) {
  bound_fns_.push(base::Bind(&RunBoundClosure, bound_closure));
}

void SerialRunner::Queue::Push(
    const BoundPipelineStatusCB& bound_status_cb) {
  bound_fns_.push(bound_status_cb);
}

SerialRunner::BoundPipelineStatusCB SerialRunner::Queue::Pop() {
  BoundPipelineStatusCB bound_fn = bound_fns_.front();
  bound_fns_.pop();
  return bound_fn;
}

bool SerialRunner::Queue::empty() {
  return bound_fns_.empty();
}

SerialRunner::SerialRunner(
    const Queue& bound_fns, const PipelineStatusCB& done_cb)
    : weak_this_(this),
      message_loop_(base::MessageLoopProxy::current()),
      bound_fns_(bound_fns),
      done_cb_(done_cb) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &SerialRunner::RunNextInSeries, weak_this_.GetWeakPtr(),
      PIPELINE_OK));
}

SerialRunner::~SerialRunner() {}

scoped_ptr<SerialRunner> SerialRunner::Run(
    const Queue& bound_fns, const PipelineStatusCB& done_cb) {
  scoped_ptr<SerialRunner> callback_series(
      new SerialRunner(bound_fns, done_cb));
  return callback_series.Pass();
}

void SerialRunner::RunNextInSeries(PipelineStatus last_status) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!done_cb_.is_null());

  if (bound_fns_.empty() || last_status != PIPELINE_OK) {
    base::ResetAndReturn(&done_cb_).Run(last_status);
    return;
  }

  BoundPipelineStatusCB bound_fn = bound_fns_.Pop();
  bound_fn.Run(base::Bind(&RunOnMessageLoop, message_loop_, base::Bind(
      &SerialRunner::RunNextInSeries, weak_this_.GetWeakPtr())));
}

}  // namespace media

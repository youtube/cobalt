// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PRIORITY_DISPATCH_H_
#define NET_BASE_PRIORITY_DISPATCH_H_
#pragma once

#include <vector>

#include "net/base/net_export.h"
#include "net/base/priority_queue.h"

namespace net {

// A priority-based dispatcher of jobs. Dispatch order is by priority (lowest
// first) and then FIFO. The dispatcher enforces limits on the number of running
// jobs. It never revokes a job once started. The job must call OnJobFinished
// once it finishes in order to dispatch further jobs.
//
// All operations are O(p) time for p priority levels. The class is fully
// reentrant: it is safe to execute any method (incl. destructor) from within
// Job callbacks. However, this class is NOT thread-safe, which is enforced
// by the underlying non-thread-safe PriorityQueue.
//
class NET_EXPORT_PRIVATE PrioritizedDispatcher {
 public:
  class Job;
  typedef PriorityQueue<Job*>::Priority Priority;

  // Describes the limits for the number of jobs started by the dispatcher.
  // For example, |total_jobs| = 30 and |reserved_slots| = { 5, 10, 5 }
  // allow for at most 30 running jobs in total. If there are already 24 jobs
  // running, then there can be 6 more jobs started of which at most 1 can be
  // at priority 1 or 0, but the rest has to be at 0.
  struct NET_EXPORT_PRIVATE Limits {
    Limits(Priority num_priorities, size_t total_jobs);
    ~Limits();

    // Total allowed running jobs.
    size_t total_jobs;
    // Number of slots reserved for each priority and higher.
    // Sum of |reserved_slots| must be no greater than |total_jobs|.
    std::vector<size_t> reserved_slots;
  };

  // An interface to the job dispatched by PrioritizedDispatcher. The dispatcher
  // does not own the Job but expects it to live as long as the Job is queued.
  // Use Cancel to remove Job from queue before it is dispatched. The Job can be
  // deleted after it is dispatched or canceled, or the dispatcher is destroyed.
  class Job {
   public:
    // Note: PriorityDispatch will never delete a Job.
    virtual ~Job() {}
    // Called when the dispatcher starts the job. Must call OnJobFinished when
    // done.
    virtual void Start() = 0;
  };

  // A handle to the enqueued job. The handle becomes invalid when the job is
  // canceled, updated, or started.
  typedef PriorityQueue<Job*>::Pointer Handle;

  // Creates a dispatcher enforcing |limits| on number of running jobs.
  PrioritizedDispatcher(const Limits& limits);

  ~PrioritizedDispatcher();

  size_t num_running_jobs() const { return num_running_jobs_; }
  size_t num_queued_jobs() const { return queue_.size(); }
  size_t num_priorities() const { return max_running_jobs_.size(); }

  // Adds |job| with |priority| to the dispatcher. If limits permit, |job| is
  // started immediately. Returns handle to the job or null-handle if the job is
  // started.
  Handle Add(Job* job, Priority priority);

  // Removes the job with |handle| from the queue. Invalidates |handle|.
  // Note: a Handle is valid iff the job is in the queue, i.e. has not Started.
  void Cancel(const Handle& handle);

  // Removes and returns the oldest-lowest Job from the queue invalidating any
  // handles to it. Returns NULL if the queue is empty.
  Job* EvictOldestLowest();

  // Moves the queued job with |handle| to the end of all values with priority
  // |priority| and returns the updated handle, or null-handle if it starts the
  // job. Invalidates |handle|. No-op if priority did not change.
  Handle ChangePriority(const Handle& handle, Priority priority);

  // Notifies the dispatcher that a running job has finished. Could start a job.
  void OnJobFinished();

 private:
  // Attempts to dispatch the job with |handle| at priority |priority| (might be
  // different than |handle.priority()|. Returns true if successful. If so
  // the |handle| becomes invalid.
  bool MaybeDispatchJob(const Handle& handle, Priority priority);

  // Queue for jobs that need to wait for a spare slot.
  PriorityQueue<Job*> queue_;
  // Maximum total number of running jobs allowed after a job at a particular
  // priority is started. If a greater or equal number of jobs are running, then
  // another job cannot be started.
  std::vector<size_t> max_running_jobs_;
  // Total number of running jobs.
  size_t num_running_jobs_;

  DISALLOW_COPY_AND_ASSIGN(PrioritizedDispatcher);
};

}  // namespace net

#endif  // NET_BASE_PRIORITY_DISPATCH_H_


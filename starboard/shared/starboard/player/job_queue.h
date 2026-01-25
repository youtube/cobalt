// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_JOB_QUEUE_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_JOB_QUEUE_H_

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/thread_checker.h"

#ifndef __cplusplus
#error "Only C++ files can include this header."
#endif

// Uncomment the following statement to enable JobQueue profiling, which will
// log the stack trace of the job that takes the longest time to execute every a
// while.
// #define ENABLE_JOB_QUEUE_PROFILING 1

namespace starboard {

// This class implements a job queue where jobs can be posted to it on any
// thread and will be processed on one thread that this job queue is linked to.
// A thread can only have one job queue.
class JobQueue {
 public:
  typedef std::function<void()> Job;

  class JobToken {
   public:
    static const int64_t kInvalidToken = -1;

    explicit JobToken(int64_t token = kInvalidToken) : token_(token) {}

    void ResetToInvalid() { token_ = kInvalidToken; }
    bool is_valid() const { return token_ != kInvalidToken; }
    bool operator==(const JobToken& that) const {
      return token_ == that.token_;
    }

   private:
    int64_t token_;
  };

  class JobOwner {
   public:
    explicit JobOwner(JobQueue* job_queue) : job_queue_(job_queue) {
      SB_CHECK(job_queue);
    }
    JobOwner(const JobOwner&) = delete;
    ~JobOwner() { CancelPendingJobs(); }

    bool BelongsToCurrentThread() const {
      return job_queue_->BelongsToCurrentThread();
    }

    JobToken Schedule(const Job& job, int64_t delay_usec = 0) {
      return job_queue_->Schedule(job, this, delay_usec);
    }
    JobToken Schedule(Job&& job, int64_t delay_usec = 0) {
      return job_queue_->Schedule(std::move(job), this, delay_usec);
    }

    void RemoveJobByToken(JobToken job_token) {
      return job_queue_->RemoveJobByToken(job_token);
    }
    void CancelPendingJobs() {
      if (job_queue_) {
        job_queue_->RemoveJobsByOwner(this);
      }
    }

   protected:
    enum DetachedState { kDetached };

    explicit JobOwner(DetachedState detached_state) : job_queue_(nullptr) {
      SB_DCHECK_EQ(detached_state, kDetached);
    }

    // Note that this operation is not thread safe.  It is the caller's
    // responsibility to ensure that concurrency hasn't happened yet.
    void Attach(JobQueue* job_queue) {
      SB_DCHECK_EQ(job_queue_, nullptr);
      SB_CHECK(job_queue);
      job_queue_ = job_queue;
    }

   private:
    JobQueue* job_queue_;
  };

  JobQueue();
  ~JobQueue();

  JobToken Schedule(const Job& job, int64_t delay_usec = 0);
  JobToken Schedule(Job&& job, int64_t delay_usec = 0);
  void ScheduleAndWait(const Job& job);
  void ScheduleAndWait(Job&& job);
  void RemoveJobByToken(JobToken job_token);

  // The processing of jobs may not be stopped when this function returns, but
  // it is guaranteed that the processing will be stopped very soon.  So it is
  // safe to join the thread after this call returns.
  void StopSoon();

  void RunUntilStopped();
  void RunUntilIdle();

  bool BelongsToCurrentThread() const;

 private:
#if ENABLE_JOB_QUEUE_PROFILING
  // Reset the max value periodically to catch all local peaks.
  static const int64_t kProfileResetIntervalUsec = 1'000'000;  // 1 second
  static const int kProfileStackDepth = 10;
#endif  // ENABLE_JOB_QUEUE_PROFILING

  struct JobRecord {
    JobToken job_token;
    Job job;
    JobOwner* owner;
#if ENABLE_JOB_QUEUE_PROFILING
    void* stack[kProfileStackDepth];
    int stack_size;
#endif  // ENABLE_JOB_QUEUE_PROFILING
  };
  typedef std::multimap<int64_t, JobRecord> TimeToJobRecordMap;

  JobToken Schedule(const Job& job, JobOwner* owner, int64_t delay_usec);
  JobToken Schedule(Job&& job, JobOwner* owner, int64_t delay_usec);
  void RemoveJobsByOwner(JobOwner* owner);
  // Return true if a job is run, otherwise return false.  When there is no job
  // ready to run currently and |wait_for_next_job| is true, the function will
  // wait to until a job is available or if the |queue_| is woken up.  Note that
  // set |wait_for_next_job| to true doesn't guarantee that one job will always
  // be run.
  bool TryToRunOneJob(bool wait_for_next_job);

  ThreadChecker thread_checker_;
  std::mutex mutex_;
  std::condition_variable condition_;
  int64_t current_job_token_ = JobToken::kInvalidToken + 1;
  TimeToJobRecordMap time_to_job_record_map_;
  bool stopped_ = false;

#if ENABLE_JOB_QUEUE_PROFILING
  int64_t last_reset_time_ = CurrentMonotonicTime();
  JobRecord job_record_with_max_interval_;
  int64_t max_job_interval_ = 0;
  int jobs_processed_ = 0;
  int wait_times_ = 0;
#endif  // ENABLE_JOB_QUEUE_PROFILING
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_JOB_QUEUE_H_

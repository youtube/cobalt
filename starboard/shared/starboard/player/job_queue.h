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
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <type_traits>
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
  // A move-only type-erased wrapper for callables with signature void().
  // This is similar to C++23 std::move_only_function. It allows capturing
  // move-only objects like std::unique_ptr.
  //
  // Lifetime: A Job object owns the wrapped callable. When the Job is
  // destroyed, the wrapped callable is also destroyed.
  //
  // Threading: A Job can be constructed and moved on any thread, but it
  // should only be invoked once. Typically, Jobs are scheduled and executed
  // on the JobQueue's thread.
  class Job {
   public:
    Job() = default;
    Job(std::nullptr_t) {}

    template <
        typename F,
        typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Job> &&
                                    std::is_invocable_v<std::decay_t<F>&>>>
    Job(F&& f) {
      if (IsNull(f)) {
        return;
      }

      using DecayedF = std::decay_t<F>;
      if constexpr (sizeof(DecayedF) <= kInlineStorageSize &&
                    alignof(DecayedF) <= kInlineStorageAlignment &&
                    std::is_nothrow_move_constructible_v<DecayedF>) {
        new (storage_.buffer) DecayedF(std::forward<F>(f));
        invoke_ = [](Buffer& buf) {
          (*reinterpret_cast<DecayedF*>(buf.buffer))();
        };
        destroy_ = [](Buffer& buf) {
          reinterpret_cast<DecayedF*>(buf.buffer)->~DecayedF();
        };
        move_ = [](Buffer& src, Buffer& dest) {
          new (dest.buffer)
              DecayedF(std::move(*reinterpret_cast<DecayedF*>(src.buffer)));
          reinterpret_cast<DecayedF*>(src.buffer)->~DecayedF();
        };
      } else {
        using Impl = ImplType<DecayedF>;
        storage_.ptr = new Impl(std::forward<F>(f));
        invoke_ = [](Buffer& buf) { static_cast<Impl*>(buf.ptr)->f_(); };
        destroy_ = [](Buffer& buf) { delete static_cast<Impl*>(buf.ptr); };
        move_ = [](Buffer& src, Buffer& dest) {
          dest.ptr = src.ptr;
          src.ptr = nullptr;
        };
      }
    }

    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

    Job(Job&& other) noexcept { MoveFrom(std::move(other)); }

    Job& operator=(Job&& other) noexcept {
      if (this != &other) {
        Reset();
        MoveFrom(std::move(other));
      }
      return *this;
    }

    ~Job() { Reset(); }

    Job& operator=(std::nullptr_t) {
      Reset();
      return *this;
    }

    void operator()() {
      SB_CHECK(invoke_);
      invoke_(storage_);
    }

    explicit operator bool() const { return invoke_ != nullptr; }

    friend bool operator==(const Job& job, std::nullptr_t) {
      return job.invoke_ == nullptr;
    }
    friend bool operator==(std::nullptr_t, const Job& job) {
      return job.invoke_ == nullptr;
    }
    friend bool operator!=(const Job& job, std::nullptr_t) {
      return job.invoke_ != nullptr;
    }
    friend bool operator!=(std::nullptr_t, const Job& job) {
      return job.invoke_ != nullptr;
    }

   private:
    // 24 bytes is large enough to fit most common callables, such as:
    // - Standard function pointers (8 bytes)
    // - Member function pointers (up to 16 bytes)
    // - Small lambdas capturing up to 3 pointers (e.g., 'this' and a couple of
    //   arguments)
    static constexpr size_t kInlineStorageSize = 24;
    static constexpr size_t kInlineStorageAlignment = alignof(std::max_align_t);

    union Buffer {
      alignas(kInlineStorageAlignment) char buffer[kInlineStorageSize];
      void* ptr;
    };

    template <typename F>
    struct ImplType {
      template <typename U>
      explicit ImplType(U&& f) : f_(std::forward<U>(f)) {}
      F f_;
    };

    void Reset() {
      if (!destroy_) {
        return;
      }

      destroy_(storage_);
      invoke_ = nullptr;
      destroy_ = nullptr;
      move_ = nullptr;
    }

    void MoveFrom(Job&& other) {
      invoke_ = other.invoke_;
      destroy_ = other.destroy_;
      move_ = other.move_;
      if (!other.move_) {
        return;
      }

      other.move_(other.storage_, storage_);
      other.invoke_ = nullptr;
      other.destroy_ = nullptr;
      other.move_ = nullptr;
    }

    template <typename T>
    static constexpr bool IsNull(const T& t) {
      if constexpr (std::is_pointer_v<T> || std::is_member_pointer_v<T>) {
        return t == nullptr;
      }
      return false;
    }

    Buffer storage_;
    // Function pointers for type-erased operations:
    void (*invoke_)(Buffer&) = nullptr;
    void (*destroy_)(Buffer&) = nullptr;
    void (*move_)(Buffer&, Buffer&) = nullptr;
  };

  class JobToken {
   public:
    static const JobToken kUnscheduled;

    explicit operator bool() const { return token_.has_value(); }

   private:
    friend class JobQueue;

    static JobToken Generate();

    JobToken() = default;
    explicit JobToken(int64_t token) : token_(token) {}

    void Reset() { token_ = std::nullopt; }

    bool operator==(const JobToken& that) const {
      return token_ == that.token_;
    }

    std::optional<int64_t> token_;
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

    JobToken Schedule(Job job, int64_t delay_usec = 0) {
      return job_queue_->Schedule(std::move(job), this, delay_usec);
    }

    void RemoveJobByToken(JobToken* job_token) {
      job_queue_->RemoveJobByToken(job_token);
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
    // This is used to associate a JobQueue with a JobOwner that was constructed
    // in a detached state (e.g. with JobOwner(kDetached)).
    void Attach(JobQueue* job_queue) {
      SB_DCHECK_EQ(job_queue_, nullptr);
      SB_CHECK(job_queue);
      job_queue_ = job_queue;
    }

    JobQueue* job_queue() const { return job_queue_; }

   private:
    JobQueue* job_queue_;
  };

  JobQueue();
  ~JobQueue();

  JobToken Schedule(Job job, int64_t delay_usec = 0);
  void ScheduleAndWait(Job job);
  void RemoveJobByToken(JobToken* job_token);

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

  JobToken Schedule(Job job, JobOwner* owner, int64_t delay_usec);
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
  TimeToJobRecordMap time_to_job_record_map_;  // Guarded by |mutex_|.
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

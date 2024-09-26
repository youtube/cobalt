// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/v8_platform.h"

#include <algorithm>

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/nonscannable_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "base/task/post_job.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "gin/per_isolate_data.h"
#include "v8_platform_page_allocator.h"

namespace gin {

namespace {

base::LazyInstance<V8Platform>::Leaky g_v8_platform = LAZY_INSTANCE_INITIALIZER;

constexpr base::TaskTraits kLowPriorityTaskTraits = {
    base::TaskPriority::BEST_EFFORT};

constexpr base::TaskTraits kDefaultTaskTraits = {
    base::TaskPriority::USER_VISIBLE};

constexpr base::TaskTraits kBlockingTaskTraits = {
    base::TaskPriority::USER_BLOCKING};

void PrintStackTrace() {
  base::debug::StackTrace trace;
  trace.Print();
}

class ConvertableToTraceFormatWrapper final
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit ConvertableToTraceFormatWrapper(
      std::unique_ptr<v8::ConvertableToTraceFormat> inner)
      : inner_(std::move(inner)) {}
  ConvertableToTraceFormatWrapper(const ConvertableToTraceFormatWrapper&) =
      delete;
  ConvertableToTraceFormatWrapper& operator=(
      const ConvertableToTraceFormatWrapper&) = delete;
  ~ConvertableToTraceFormatWrapper() override = default;
  void AppendAsTraceFormat(std::string* out) const final {
    inner_->AppendAsTraceFormat(out);
  }

 private:
  std::unique_ptr<v8::ConvertableToTraceFormat> inner_;
};

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
class EnabledStateObserverImpl final
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  EnabledStateObserverImpl() {
    base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
  }

  EnabledStateObserverImpl(const EnabledStateObserverImpl&) = delete;

  EnabledStateObserverImpl& operator=(const EnabledStateObserverImpl&) = delete;

  ~EnabledStateObserverImpl() override {
    base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(
        this);
  }

  void OnTraceLogEnabled() final {
    base::AutoLock lock(mutex_);
    for (auto* o : observers_) {
      o->OnTraceEnabled();
    }
  }

  void OnTraceLogDisabled() final {
    base::AutoLock lock(mutex_);
    for (auto* o : observers_) {
      o->OnTraceDisabled();
    }
  }

  void AddObserver(v8::TracingController::TraceStateObserver* observer) {
    {
      base::AutoLock lock(mutex_);
      DCHECK(!observers_.count(observer));
      observers_.insert(observer);
    }

    // Fire the observer if recording is already in progress.
    if (base::trace_event::TraceLog::GetInstance()->IsEnabled())
      observer->OnTraceEnabled();
  }

  void RemoveObserver(v8::TracingController::TraceStateObserver* observer) {
    base::AutoLock lock(mutex_);
    DCHECK(observers_.count(observer) == 1);
    observers_.erase(observer);
  }

 private:
  base::Lock mutex_;
  std::unordered_set<v8::TracingController::TraceStateObserver*> observers_;
};

base::LazyInstance<EnabledStateObserverImpl>::Leaky g_trace_state_dispatcher =
    LAZY_INSTANCE_INITIALIZER;
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

#if BUILDFLAG(USE_PARTITION_ALLOC)

base::LazyInstance<gin::PageAllocator>::Leaky g_page_allocator =
    LAZY_INSTANCE_INITIALIZER;

#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

class JobDelegateImpl : public v8::JobDelegate {
 public:
  explicit JobDelegateImpl(base::JobDelegate* delegate) : delegate_(delegate) {}
  JobDelegateImpl() = default;

  JobDelegateImpl(const JobDelegateImpl&) = delete;
  JobDelegateImpl& operator=(const JobDelegateImpl&) = delete;

  // v8::JobDelegate:
  bool ShouldYield() override { return delegate_->ShouldYield(); }
  void NotifyConcurrencyIncrease() override {
    delegate_->NotifyConcurrencyIncrease();
  }
  uint8_t GetTaskId() override { return delegate_->GetTaskId(); }
  bool IsJoiningThread() const override { return delegate_->IsJoiningThread(); }

 private:
  raw_ptr<base::JobDelegate> delegate_;
};

class JobHandleImpl : public v8::JobHandle {
 public:
  explicit JobHandleImpl(base::JobHandle handle) : handle_(std::move(handle)) {}
  ~JobHandleImpl() override = default;

  JobHandleImpl(const JobHandleImpl&) = delete;
  JobHandleImpl& operator=(const JobHandleImpl&) = delete;

  // v8::JobHandle:
  void NotifyConcurrencyIncrease() override {
    handle_.NotifyConcurrencyIncrease();
  }
  bool UpdatePriorityEnabled() const override { return true; }
  void UpdatePriority(v8::TaskPriority new_priority) override {
    handle_.UpdatePriority(ToBaseTaskPriority(new_priority));
  }
  void Join() override { handle_.Join(); }
  void Cancel() override { handle_.Cancel(); }
  void CancelAndDetach() override { handle_.CancelAndDetach(); }
  bool IsActive() override { return handle_.IsActive(); }
  bool IsValid() override { return !!handle_; }

 private:
  static base::TaskPriority ToBaseTaskPriority(v8::TaskPriority priority) {
    switch (priority) {
      case v8::TaskPriority::kBestEffort:
        return base::TaskPriority::BEST_EFFORT;
      case v8::TaskPriority::kUserVisible:
        return base::TaskPriority::USER_VISIBLE;
      case v8::TaskPriority::kUserBlocking:
        return base::TaskPriority::USER_BLOCKING;
    }
  }

  base::JobHandle handle_;
};

class ScopedBlockingCallImpl : public v8::ScopedBlockingCall {
 public:
  explicit ScopedBlockingCallImpl(v8::BlockingType blocking_type)
      : scoped_blocking_call_(ToBaseBlockingType(blocking_type),
                              base::internal::UncheckedScopedBlockingCall::
                                  BlockingCallType::kRegular) {}
  ~ScopedBlockingCallImpl() override = default;

  ScopedBlockingCallImpl(const ScopedBlockingCallImpl&) = delete;
  ScopedBlockingCallImpl& operator=(const ScopedBlockingCallImpl&) = delete;

 private:
  static base::BlockingType ToBaseBlockingType(v8::BlockingType type) {
    switch (type) {
      case v8::BlockingType::kMayBlock:
        return base::BlockingType::MAY_BLOCK;
      case v8::BlockingType::kWillBlock:
        return base::BlockingType::WILL_BLOCK;
    }
  }

  base::internal::UncheckedScopedBlockingCall scoped_blocking_call_;
};

}  // namespace

}  // namespace gin

// Allow std::unique_ptr<v8::ConvertableToTraceFormat> to be a valid
// initialization value for trace macros.
template <>
struct base::trace_event::TraceValue::Helper<
    std::unique_ptr<v8::ConvertableToTraceFormat>> {
  static constexpr unsigned char kType = TRACE_VALUE_TYPE_CONVERTABLE;
  static inline void SetValue(
      TraceValue* v,
      std::unique_ptr<v8::ConvertableToTraceFormat> value) {
    // NOTE: |as_convertable| is an owning pointer, so using new here
    // is acceptable.
    v->as_convertable =
        new gin::ConvertableToTraceFormatWrapper(std::move(value));
  }
};

namespace gin {

class V8Platform::TracingControllerImpl : public v8::TracingController {
 public:
  TracingControllerImpl() = default;
  TracingControllerImpl(const TracingControllerImpl&) = delete;
  TracingControllerImpl& operator=(const TracingControllerImpl&) = delete;
  ~TracingControllerImpl() override = default;

  // TracingController implementation.
#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(name);
  }
  uint64_t AddTraceEvent(
      char phase,
      const uint8_t* category_enabled_flag,
      const char* name,
      const char* scope,
      uint64_t id,
      uint64_t bind_id,
      int32_t num_args,
      const char** arg_names,
      const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) override {
    base::trace_event::TraceArguments args(
        num_args, arg_names, arg_types,
        reinterpret_cast<const unsigned long long*>(arg_values),
        arg_convertables);
    DCHECK_LE(num_args, 2);
    base::trace_event::TraceEventHandle handle =
        TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID(
            phase, category_enabled_flag, name, scope, id, bind_id, &args,
            flags);
    uint64_t result;
    memcpy(&result, &handle, sizeof(result));
    return result;
  }
  uint64_t AddTraceEventWithTimestamp(
      char phase,
      const uint8_t* category_enabled_flag,
      const char* name,
      const char* scope,
      uint64_t id,
      uint64_t bind_id,
      int32_t num_args,
      const char** arg_names,
      const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags,
      int64_t timestampMicroseconds) override {
    base::trace_event::TraceArguments args(
        num_args, arg_names, arg_types,
        reinterpret_cast<const unsigned long long*>(arg_values),
        arg_convertables);
    DCHECK_LE(num_args, 2);
    base::TimeTicks timestamp =
        base::TimeTicks() + base::Microseconds(timestampMicroseconds);
    base::trace_event::TraceEventHandle handle =
        TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
            phase, category_enabled_flag, name, scope, id, bind_id,
            TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, &args, flags);
    uint64_t result;
    memcpy(&result, &handle, sizeof(result));
    return result;
  }
  void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                const char* name,
                                uint64_t handle) override {
    base::trace_event::TraceEventHandle traceEventHandle;
    memcpy(&traceEventHandle, &handle, sizeof(handle));
    TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(category_enabled_flag, name,
                                                traceEventHandle);
  }
  void AddTraceStateObserver(TraceStateObserver* observer) override {
    g_trace_state_dispatcher.Get().AddObserver(observer);
  }
  void RemoveTraceStateObserver(TraceStateObserver* observer) override {
    g_trace_state_dispatcher.Get().RemoveObserver(observer);
  }
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
};

// static
V8Platform* V8Platform::Get() { return g_v8_platform.Pointer(); }

V8Platform::V8Platform() : tracing_controller_(new TracingControllerImpl) {}

V8Platform::~V8Platform() = default;

#if BUILDFLAG(USE_PARTITION_ALLOC)
PageAllocator* V8Platform::GetPageAllocator() {
  return g_page_allocator.Pointer();
}

void V8Platform::OnCriticalMemoryPressure() {
// We only have a reservation on 32-bit Windows systems.
// TODO(bbudge) Make the #if's in BlinkInitializer match.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_32_BITS)
  partition_alloc::ReleaseReservation();
#endif
}

v8::ZoneBackingAllocator* V8Platform::GetZoneBackingAllocator() {
  static struct Allocator final : v8::ZoneBackingAllocator {
    MallocFn GetMallocFn() const override {
      return &base::AllocNonQuarantinable;
    }
    FreeFn GetFreeFn() const override { return &base::FreeNonQuarantinable; }
  } allocator;
  return &allocator;
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

std::shared_ptr<v8::TaskRunner> V8Platform::GetForegroundTaskRunner(
    v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  return data->task_runner();
}

int V8Platform::NumberOfWorkerThreads() {
  // V8Platform assumes the number of workers used by the scheduler for user
  // blocking tasks is an upper bound.
  const size_t num_foreground_workers =
      base::ThreadPoolInstance::Get()
          ->GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
              kBlockingTaskTraits);
  DCHECK_GE(num_foreground_workers,
            base::ThreadPoolInstance::Get()
                ->GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                    kDefaultTaskTraits));
  return std::max(1, static_cast<int>(num_foreground_workers));
}

void V8Platform::CallOnWorkerThread(std::unique_ptr<v8::Task> task) {
  base::ThreadPool::PostTask(FROM_HERE, kDefaultTaskTraits,
                             base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8Platform::CallBlockingTaskOnWorkerThread(
    std::unique_ptr<v8::Task> task) {
  base::ThreadPool::PostTask(FROM_HERE, kBlockingTaskTraits,
                             base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8Platform::CallLowPriorityTaskOnWorkerThread(
    std::unique_ptr<v8::Task> task) {
  base::ThreadPool::PostTask(FROM_HERE, kLowPriorityTaskTraits,
                             base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8Platform::CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task,
                                           double delay_in_seconds) {
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, kDefaultTaskTraits,
      base::BindOnce(&v8::Task::Run, std::move(task)),
      base::Seconds(delay_in_seconds));
}

std::unique_ptr<v8::JobHandle> V8Platform::CreateJob(
    v8::TaskPriority priority,
    std::unique_ptr<v8::JobTask> job_task) {
  base::TaskTraits task_traits;
  switch (priority) {
    case v8::TaskPriority::kBestEffort:
      task_traits = kLowPriorityTaskTraits;
      break;
    case v8::TaskPriority::kUserVisible:
      task_traits = kDefaultTaskTraits;
      break;
    case v8::TaskPriority::kUserBlocking:
      task_traits = kBlockingTaskTraits;
      break;
  }
  // Ownership of |job_task| is assumed by |worker_task|, while
  // |max_concurrency_callback| uses an unretained pointer.
  auto* job_task_ptr = job_task.get();
  auto handle =
      base::CreateJob(FROM_HERE, task_traits,
                      base::BindRepeating(
                          [](const std::unique_ptr<v8::JobTask>& job_task,
                             base::JobDelegate* delegate) {
                            JobDelegateImpl delegate_impl(delegate);
                            job_task->Run(&delegate_impl);
                          },
                          std::move(job_task)),
                      base::BindRepeating(
                          [](v8::JobTask* job_task, size_t worker_count) {
                            return job_task->GetMaxConcurrency(worker_count);
                          },
                          base::Unretained(job_task_ptr)));

  return std::make_unique<JobHandleImpl>(std::move(handle));
}

std::unique_ptr<v8::ScopedBlockingCall> V8Platform::CreateBlockingScope(
    v8::BlockingType blocking_type) {
  return std::make_unique<ScopedBlockingCallImpl>(blocking_type);
}

bool V8Platform::IdleTasksEnabled(v8::Isolate* isolate) {
  return PerIsolateData::From(isolate)->task_runner()->IdleTasksEnabled();
}

double V8Platform::MonotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

double V8Platform::CurrentClockTimeMillis() {
  return static_cast<double>(time_clamper_.ClampToMillis(base::Time::Now()));
}

int64_t V8Platform::CurrentClockTimeMilliseconds() {
  return time_clamper_.ClampToMillis(base::Time::Now());
}

double V8Platform::CurrentClockTimeMillisecondsHighResolution() {
  return time_clamper_.ClampToMillisHighResolution(base::Time::Now());
}

v8::TracingController* V8Platform::GetTracingController() {
  return tracing_controller_.get();
}

v8::Platform::StackTracePrinter V8Platform::GetStackTracePrinter() {
  return PrintStackTrace;
}

}  // namespace gin

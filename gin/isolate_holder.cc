// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/isolate_holder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gin/debug_impl.h"
#include "v8/include/v8-statistics.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/v8_initializer.h"
#include "gin/v8_isolate_memory_dump_provider.h"
#include "gin/v8_shared_memory_dump_provider.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-locker.h"
#include "v8/include/v8-snapshot.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "base/no_destructor.h"
#include "base/threading/thread_local.h"

namespace gin {

namespace {
v8::ArrayBuffer::Allocator* g_array_buffer_allocator = nullptr;
const intptr_t* g_reference_table = nullptr;
v8::FatalErrorCallback g_fatal_error_callback = nullptr;
v8::OOMErrorCallback g_oom_error_callback = nullptr;

// Thread-local storage to safely track GC start times across multiple V8 threads
base::ThreadLocalOwnedPointer<base::TimeTicks>& GetGCStartTime() {
  static base::NoDestructor<base::ThreadLocalOwnedPointer<base::TimeTicks>> instance;
  return *instance;
}

// Helper to convert V8 GCType enum to a human-readable string
const char* GCTypeToString(v8::GCType type) {
  switch (type) {
    case v8::kGCTypeScavenge:
      return "Scavenge";
    case v8::kGCTypeMarkSweepCompact:
      return "MarkSweepCompact";
    case v8::kGCTypeIncrementalMarking:
      return "IncrementalMarking";
    case v8::kGCTypeProcessWeakCallbacks:
      return "ProcessWeakCallbacks";
    default:
      return "Unknown";
  }
}

// 1. Prologue Callback: Triggered by V8 right before a GC cycle starts
void V8GCPrologueCallback(v8::Isolate* isolate,
                          v8::GCType type,
                          v8::GCCallbackFlags flags,
                          void* data) {
  LOG(INFO) << "V8_GC_PROLOGUE_TRIGGERED: type=" << GCTypeToString(type);
  // Capture the high-resolution start timestamp
  GetGCStartTime().Set(std::make_unique<base::TimeTicks>(base::TimeTicks::Now()));
}

// 2. Epilogue Callback: Triggered by V8 right after a GC cycle completes
void V8GCEpilogueCallback(v8::Isolate* isolate,
                          v8::GCType type,
                          v8::GCCallbackFlags flags,
                          void* data) {
  LOG(INFO) << "V8_GC_EPILOGUE_TRIGGERED: type=" << GCTypeToString(type);
  base::TimeTicks* start_time = GetGCStartTime().Get();
  if (!start_time) {
    LOG(WARNING) << "V8_GC_EPILOGUE: start_time is null!";
    return;
  }

  // Calculate the exact pause duration
  base::TimeDelta duration = base::TimeTicks::Now() - *start_time;
  const char* type_str = GCTypeToString(type);

  // LOG(INFO) is guaranteed to write directly to Android logcat under 'cobalt' tag!
  LOG(INFO) << "V8_GC_EVENT: type=" << type_str
            << " duration_ms=" << duration.InMillisecondsF();
  // Clear the thread-local timestamp
  GetGCStartTime().Set(nullptr);
}

std::unique_ptr<v8::Isolate::CreateParams> getModifiedIsolateParams(
    std::unique_ptr<v8::Isolate::CreateParams> params,
    IsolateHolder::AllowAtomicsWaitMode atomics_wait_mode,
    v8::CreateHistogramCallback create_histogram_callback,
    v8::AddHistogramSampleCallback add_histogram_sample_callback,
    std::unique_ptr<v8::CppHeap> cpp_heap) {
  params->create_histogram_callback = create_histogram_callback;
  params->add_histogram_sample_callback = add_histogram_sample_callback;
  params->allow_atomics_wait =
      atomics_wait_mode ==
      IsolateHolder::AllowAtomicsWaitMode::kAllowAtomicsWait;
  params->array_buffer_allocator = g_array_buffer_allocator;
  // V8 takes ownership of the CppHeap.
  params->cpp_heap = cpp_heap.release();
  return params;
}

std::string LocalFormatWithDigitSeparators(uint64_t number) {
  std::string s = std::to_string(number);
  int n = s.length();
  for (int i = n - 3; i > 0; i -= 3) {
    s.insert(i, 1, '\'');
  }
  return s;
}

}  // namespace

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    IsolateType isolate_type)
    : IsolateHolder(std::move(task_runner),
                    AccessMode::kSingleThread,
                    isolate_type) {}

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AccessMode access_mode,
    IsolateType isolate_type)
    : IsolateHolder(std::move(task_runner),
                    access_mode,
                    kAllowAtomicsWait,
                    isolate_type,
                    IsolateCreationMode::kNormal) {}

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AccessMode access_mode,
    AllowAtomicsWaitMode atomics_wait_mode,
    IsolateType isolate_type,
    IsolateCreationMode isolate_creation_mode,
    v8::CreateHistogramCallback create_histogram_callback,
    v8::AddHistogramSampleCallback add_histogram_sample_callback,
    scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner,
    std::unique_ptr<v8::CppHeap> cpp_heap)
    : IsolateHolder(std::move(task_runner),
                    access_mode,
                    isolate_type,
                    getModifiedIsolateParams(getDefaultIsolateParams(),
                                             atomics_wait_mode,
                                             create_histogram_callback,
                                             add_histogram_sample_callback,
                                             std::move(cpp_heap)),
                    isolate_creation_mode,
                    std::move(user_visible_task_runner),
                    std::move(best_effort_task_runner)) {}

IsolateHolder::IsolateHolder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    AccessMode access_mode,
    IsolateType isolate_type,
    std::unique_ptr<v8::Isolate::CreateParams> params,
    IsolateCreationMode isolate_creation_mode,
    scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner)
    : memory_pressure_count_(0),
      access_mode_(access_mode),
      isolate_type_(isolate_type) {
  CHECK(Initialized())
      << "You need to invoke gin::IsolateHolder::Initialize first";

  DCHECK(task_runner);
  DCHECK(task_runner->BelongsToCurrentThread());

  v8::ArrayBuffer::Allocator* allocator = params->array_buffer_allocator;
  DCHECK(allocator);

  isolate_ = v8::Isolate::Allocate();
  isolate_data_ = std::make_unique<PerIsolateData>(
      isolate_, allocator, access_mode_, task_runner,
      std::move(user_visible_task_runner), std::move(best_effort_task_runner));
  if (isolate_creation_mode == IsolateCreationMode::kCreateSnapshot) {
    // This branch is called when creating a V8 snapshot for Blink.
    // SnapshotCreator initializes the Isolate using the CreateParams and calls
    // isolate->Enter() in its construction. The Isolate is still owned by the
    // IsolateHolder.
    params->external_references = g_reference_table;
    snapshot_creator_ =
        std::make_unique<v8::SnapshotCreator>(isolate_, *params);
    DCHECK_EQ(isolate_, snapshot_creator_->GetIsolate());
  } else {
    v8::Isolate::Initialize(isolate_, *params);
  }

  // Register native V8 GC telemetry callbacks
  isolate_->AddGCPrologueCallback(V8GCPrologueCallback);
  isolate_->AddGCEpilogueCallback(V8GCEpilogueCallback);

  // This will attempt register the shared memory dump provider for every
  // IsolateHolder, but only the first registration will have any effect.
  gin::V8SharedMemoryDumpProvider::Register();

  isolate_memory_dump_provider_ =
      std::make_unique<V8IsolateMemoryDumpProvider>(this, task_runner);

  // Register memory pressure listener
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&IsolateHolder::RecordMemoryPressure,
                          base::Unretained(this)));

  // Start periodic logging
  PeriodicallyLogV8HeapAndPressure(task_runner);
}

IsolateHolder::~IsolateHolder() {
  isolate_memory_dump_provider_.reset();
  {
    std::optional<v8::Locker> locker;
    if (access_mode_ == AccessMode::kUseLocker) {
      locker.emplace(isolate_);
    }
    v8::Isolate::Scope isolate_scope(isolate_);
    isolate_data_->NotifyBeforeDispose();
  }
  // Calling Isolate::Dispose makes sure all threads which might access
  // PerIsolateData are finished.
  isolate_->Dispose();
  isolate_data_->NotifyDisposed();
  isolate_data_.reset();
  isolate_ = nullptr;
}

// static
void IsolateHolder::Initialize(ScriptMode mode,
                               v8::ArrayBuffer::Allocator* allocator,
                               const intptr_t* reference_table,
                               std::string js_command_line_flags,
                               bool disallow_v8_feature_flag_overrides,
                               v8::FatalErrorCallback fatal_error_callback,
                               v8::OOMErrorCallback oom_error_callback) {
  CHECK(allocator);
  V8Initializer::Initialize(mode, js_command_line_flags,
                            disallow_v8_feature_flag_overrides,
                            oom_error_callback);
  g_array_buffer_allocator = allocator;
  g_reference_table = reference_table;
  g_fatal_error_callback = fatal_error_callback;
  g_oom_error_callback = oom_error_callback;
}

// static
bool IsolateHolder::Initialized() {
  return g_array_buffer_allocator;
}

// static
std::unique_ptr<v8::Isolate::CreateParams>
IsolateHolder::getDefaultIsolateParams() {
  CHECK(Initialized())
      << "You need to invoke gin::IsolateHolder::Initialize first";
  // TODO(https://crbug.com/v8/13092): Remove usage of unique_ptr once V8
  // supports the move constructor on CreateParams.
  std::unique_ptr<v8::Isolate::CreateParams> params =
      std::make_unique<v8::Isolate::CreateParams>();
  params->code_event_handler = DebugImpl::GetJitCodeEventHandler();
  params->constraints.ConfigureDefaults(base::SysInfo::AmountOfPhysicalMemory(),
                                        base::SysInfo::AmountOfVirtualMemory());
  params->array_buffer_allocator = g_array_buffer_allocator;
  params->allow_atomics_wait = true;
  params->external_references = g_reference_table;
  params->embedder_wrapper_type_index = kWrapperInfoIndex;
  params->embedder_wrapper_object_index = kEncodedValueIndex;
  params->fatal_error_callback = g_fatal_error_callback;
  params->oom_error_callback = g_oom_error_callback;
  return params;
}

void IsolateHolder::EnableIdleTasks(
    std::unique_ptr<V8IdleTaskRunner> idle_task_runner) {
  DCHECK(isolate_data_.get());
  isolate_data_->EnableIdleTasks(std::move(idle_task_runner));
}

void IsolateHolder::RecordMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel level) {
  memory_pressure_count_.fetch_add(1, std::memory_order_relaxed);
}

void IsolateHolder::PeriodicallyLogV8HeapAndPressure(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!isolate_) return;

  v8::HeapStatistics stats;
  isolate_->GetHeapStatistics(&stats);

  int pressure_count = memory_pressure_count_.exchange(0, std::memory_order_relaxed);

  uint64_t used_kib = stats.used_heap_size() / 1024;
  uint64_t total_kib = stats.total_heap_size() / 1024;
  uint64_t limit_kib = stats.heap_size_limit() / 1024;
  uint64_t executable_kib = stats.total_heap_size_executable() / 1024;
  uint64_t physical_kib = stats.total_physical_size() / 1024;
  uint64_t external_kib = stats.external_memory() / 1024;

  LOG(INFO) << "V8_HEAP_STATS: used(KiB)=" << LocalFormatWithDigitSeparators(used_kib)
            << ", total(KiB)=" << LocalFormatWithDigitSeparators(total_kib)
            << ", physical(KiB)=" << LocalFormatWithDigitSeparators(physical_kib)
            << ", executable(KiB)=" << LocalFormatWithDigitSeparators(executable_kib)
            << ", external(KiB)=" << LocalFormatWithDigitSeparators(external_kib)
            << ", limit(KiB)=" << LocalFormatWithDigitSeparators(limit_kib)
            << ", pressure_signals_received=" << pressure_count;

  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&IsolateHolder::PeriodicallyLogV8HeapAndPressure,
                     base::Unretained(this), task_runner),
      base::Seconds(5));
}

}  // namespace gin

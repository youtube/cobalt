// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/script/v8c/v8c_engine.h"

#include <algorithm>
#include <string>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "cobalt/base/c_val.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/script/v8c/isolate_fellowship.h"
#include "cobalt/script/v8c/v8c_global_environment.h"

namespace cobalt {
namespace script {
namespace v8c {

V8cEngine::V8cEngine(const Options& options)
    : accumulated_extra_memory_cost_(0), options_(options) {
  TRACE_EVENT0("cobalt::script", "V8cEngine::V8cEngine()");

  auto* isolate_fellowship = IsolateFellowship::GetInstance();

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      isolate_fellowship->array_buffer_allocator;
  auto* startup_data = &isolate_fellowship->startup_data;
  if (startup_data->data != nullptr) {
    create_params.snapshot_blob = startup_data;
  } else {
    // Technically possible to attempt to recover here, but hitting this
    // indicates that something is probably seriously wrong.
    LOG(WARNING) << "Isolate fellowship startup data was null, this will "
                    "significantly slow down startup time.";
  }

  isolate_ = v8::Isolate::New(create_params);
  CHECK(isolate_);

  // There are 2 total isolate data slots, one for the sole |V8cEngine| (us),
  // and one for the |V8cGlobalEnvironment|.
  const int kTotalIsolateDataSlots = 2;
  DCHECK_GE(v8::Isolate::GetNumberOfDataSlots(), kTotalIsolateDataSlots);
  isolate_->SetData(kIsolateDataIndex, this);

  v8c_heap_tracer_.reset(new V8cHeapTracer(isolate_));
  isolate_->SetEmbedderHeapTracer(v8c_heap_tracer_.get());
}

V8cEngine::~V8cEngine() {
  TRACE_EVENT0("cobalt::script", "V8cEngine::~V8cEngine");
  DCHECK(thread_checker_.CalledOnValidThread());

  // This next GC is to GC everything (mostly for ASan), so we actually don't
  // want our wrappers and wrappables to stay alive.
  isolate_->SetEmbedderHeapTracer(nullptr);

  // Send a low memory notification to V8 in order to force a garbage
  // collection before shut down.  This is required to run weak callbacks that
  // are responsible for freeing native objects that live in the internal
  // fields of V8 objects.  In the future, we should consider investigating if
  // there are startup performance wins to be made by delaying this work until
  // later (e.g., does the GC of the splash screen web module dying hurt us?).
  isolate_->LowMemoryNotification();
  isolate_->Dispose();
  isolate_ = nullptr;
}

scoped_refptr<GlobalEnvironment> V8cEngine::CreateGlobalEnvironment() {
  TRACE_EVENT0("cobalt::script", "V8cEngine::CreateGlobalEnvironment()");
  DCHECK(thread_checker_.CalledOnValidThread());
  return new V8cGlobalEnvironment(isolate_);
}

void V8cEngine::CollectGarbage() {
  TRACE_EVENT0("cobalt::script", "V8cEngine::CollectGarbage()");
  DCHECK(thread_checker_.CalledOnValidThread());
  isolate_->LowMemoryNotification();
}

void V8cEngine::ReportExtraMemoryCost(size_t bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  accumulated_extra_memory_cost_ += bytes;

  const bool do_collect_garbage =
      accumulated_extra_memory_cost_ > options_.gc_threshold_bytes;
  if (do_collect_garbage) {
    accumulated_extra_memory_cost_ = 0;
    CollectGarbage();
  }
}

bool V8cEngine::RegisterErrorHandler(JavaScriptEngine::ErrorHandler handler) {
  error_handler_ = handler;
  return true;
}

void V8cEngine::SetGcThreshold(int64_t bytes) { NOTIMPLEMENTED(); }

HeapStatistics V8cEngine::GetHeapStatistics() {
  v8::HeapStatistics v8_heap_statistics;
  isolate_->GetHeapStatistics(&v8_heap_statistics);
  return {v8_heap_statistics.total_heap_size(),
          v8_heap_statistics.used_heap_size()};
}

}  // namespace v8c

// static
scoped_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine(
    const JavaScriptEngine::Options& options) {
  TRACE_EVENT0("cobalt::script", "JavaScriptEngine::CreateEngine()");
  return make_scoped_ptr<JavaScriptEngine>(new v8c::V8cEngine(options));
}

}  // namespace script
}  // namespace cobalt

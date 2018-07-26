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
#include "starboard/once.h"

namespace cobalt {
namespace script {
namespace v8c {

namespace {

void VisitWeakHandlesForMinorGC(v8::Isolate* isolate) {
  class V8cPersistentHandleVisitor : public v8::PersistentHandleVisitor {
   public:
    void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                               uint16_t class_id) override {
      DCHECK(value);
      value->MarkActive();
    }
  } visitor;
  isolate->VisitWeakHandles(&visitor);
}

size_t UsedHeapSize(v8::Isolate* isolate) {
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  return heap_statistics.used_heap_size();
}

void GCPrologueCallback(v8::Isolate* isolate, v8::GCType type,
                        v8::GCCallbackFlags) {
  switch (type) {
    case v8::kGCTypeScavenge:
      TRACE_EVENT_BEGIN1("cobalt::script", "MinorGC", "usedHeapSizeBefore",
                         UsedHeapSize(isolate));
      VisitWeakHandlesForMinorGC(isolate);
      break;
    case v8::kGCTypeMarkSweepCompact:
      TRACE_EVENT_BEGIN2("cobalt::script", "MajorGC", "usedHeapSizeBefore",
                         UsedHeapSize(isolate), "type", "atomic pause");
      break;
    case v8::kGCTypeIncrementalMarking:
      TRACE_EVENT_BEGIN2("cobalt::script", "MajorGC", "usedHeapSizeBefore",
                         UsedHeapSize(isolate), "type", "incremental marking");
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_BEGIN2("cobalt::script", "MajorGC", "usedHeapSizeBefore",
                         UsedHeapSize(isolate), "type", "weak processing");
      break;
    default:
      NOTREACHED();
  }
}

void GCEpilogueCallback(v8::Isolate* isolate, v8::GCType type,
                        v8::GCCallbackFlags) {
  switch (type) {
    case v8::kGCTypeScavenge:
      TRACE_EVENT_END1("cobalt::script", "MinorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    case v8::kGCTypeMarkSweepCompact:
      TRACE_EVENT_END1("cobalt::script", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    case v8::kGCTypeIncrementalMarking:
      TRACE_EVENT_END1("cobalt::script", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_END1("cobalt::script", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    default:
      NOTREACHED();
  }
}

SbOnceControl v8_flag_init_once_control = SB_ONCE_INITIALIZER;

// Configure v8's global command line flag options for Cobalt.
void V8FlagInitOnce() {
  char optimize_for_size_flag_str[] = "--optimize_for_size=true";
  v8::V8::SetFlagsFromString(optimize_for_size_flag_str,
                             sizeof(optimize_for_size_flag_str));
}

}  // namespace

V8cEngine::V8cEngine(const Options& options) : options_(options) {
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

  SbOnce(&v8_flag_init_once_control, V8FlagInitOnce);
  isolate_ = v8::Isolate::New(create_params);
  CHECK(isolate_);

  // There are 2 total isolate data slots, one for the sole |V8cEngine| (us),
  // and one for the |V8cGlobalEnvironment|.
  const int kTotalIsolateDataSlots = 2;
  DCHECK_GE(v8::Isolate::GetNumberOfDataSlots(), kTotalIsolateDataSlots);
  isolate_->SetData(kIsolateDataIndex, this);

  v8c_heap_tracer_.reset(new V8cHeapTracer(isolate_));
  isolate_->SetEmbedderHeapTracer(v8c_heap_tracer_.get());

  isolate_->AddGCPrologueCallback(GCPrologueCallback);
  isolate_->AddGCEpilogueCallback(GCEpilogueCallback);

  // The V8 |SetStackLimit|'s parameter is the memory address that it should not
  // pass, as opposed to the size of the stack that it should use.  We set it
  // to 3/4 of the main thread's stack size to cover for the space underneath
  // the stack currently, and the space we want to reserve on top of the stack
  // for when JavaScript calls back into C++ bindings.
  uintptr_t here = reinterpret_cast<uintptr_t>(&here);
  isolate_->SetStackLimit(here -
                          (3 * cobalt::browser::kWebModuleStackSize) / 4);
}

V8cEngine::~V8cEngine() {
  TRACE_EVENT0("cobalt::script", "V8cEngine::~V8cEngine");
  DCHECK(thread_checker_.CalledOnValidThread());

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

void V8cEngine::AdjustAmountOfExternalAllocatedMemory(int64_t bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  isolate_->AdjustAmountOfExternalAllocatedMemory(bytes);
}

bool V8cEngine::RegisterErrorHandler(JavaScriptEngine::ErrorHandler handler) {
  error_handler_ = handler;
  return true;
}

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

std::string GetJavaScriptEngineNameAndVersion() {
  return std::string("v8/") + v8::V8::GetVersion();
}

}  // namespace script
}  // namespace cobalt

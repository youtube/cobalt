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
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/c_val.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/script/v8c/isolate_fellowship.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "starboard/once.h"

namespace cobalt {
namespace script {
namespace v8c {

namespace {

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

void ErrorMessageListener(v8::Local<v8::Message> message,
                          v8::Local<v8::Value> data) {
  v8::Isolate* isolate = message->GetIsolate();
  std::string description(*v8::String::Utf8Value(isolate, message->Get()));

  v8::Local<v8::StackTrace> stack = message->GetStackTrace();
  for (int i = 0; i < stack->GetFrameCount(); ++i) {
    v8::Local<v8::StackFrame> frame = stack->GetFrame(isolate, i);
    description += "\n";
    v8::String::Utf8Value function_name(isolate, frame->GetFunctionName());
    v8::String::Utf8Value script_name(isolate, frame->GetScriptName());
    if (*script_name) {
      description += *script_name;
    } else {
      description += "unknown";
    }
    if (*function_name) {
      description += "(";
      description += *function_name;
      description += ")";
    } else {
      description += "(unknown)";
    }
    description += ":";
    description += std::to_string(frame->GetLineNumber());
    description += ":";
    description += std::to_string(frame->GetColumn());
  }

  // TODO: Send the description to the console instead of logging it.
  LOG(ERROR) << description;
}

}  // namespace

V8cEngine::V8cEngine(const Options& options) : options_(options) {
  TRACE_EVENT0("cobalt::script", "V8cEngine::V8cEngine()");

  auto* isolate_fellowship = IsolateFellowship::GetInstance();

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      isolate_fellowship->array_buffer_allocator;

  isolate_ = v8::Isolate::New(create_params);
  CHECK(isolate_);
  isolate_fellowship->platform->RegisterIsolateOnThread(
      isolate_, base::MessageLoop::current());

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

#if !defined(COBALT_BUILD_TYPE_GOLD)
  // Report callstacks for exceptions.
  isolate_->AddMessageListener(&ErrorMessageListener);
  isolate_->SetCaptureStackTraceForUncaughtExceptions(true);
#endif
}

V8cEngine::~V8cEngine() {
  TRACE_EVENT0("cobalt::script", "V8cEngine::~V8cEngine");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  IsolateFellowship::GetInstance()->platform->UnregisterIsolateOnThread(
      isolate_);
  DCHECK(!isolate_->InContext());  // global object must be out of scope now.
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return new V8cGlobalEnvironment(isolate_);
}

void V8cEngine::CollectGarbage() {
  TRACE_EVENT0("cobalt::script", "V8cEngine::CollectGarbage()");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  isolate_->LowMemoryNotification();
}

void V8cEngine::AdjustAmountOfExternalAllocatedMemory(int64_t bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void V8cEngine::UpdateDateTimeConfiguration() {
  isolate_->DateTimeConfigurationChangeNotification(
      v8::Isolate::TimeZoneDetection::kRedetect);
}

}  // namespace v8c

// static
std::unique_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine(
    const JavaScriptEngine::Options& options) {
  TRACE_EVENT0("cobalt::script", "JavaScriptEngine::CreateEngine()");
  return std::unique_ptr<JavaScriptEngine>(new v8c::V8cEngine(options));
}

std::string GetJavaScriptEngineNameAndVersion() {
  static std::string jit_flag =
      configuration::Configuration::GetInstance()->CobaltEnableJit()
          ? "-jit"
          : "-jitless";
  return std::string("v8/") + v8::V8::GetVersion() + jit_flag;
}

}  // namespace script
}  // namespace cobalt

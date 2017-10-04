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
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "starboard/once.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {

namespace v8c {
namespace {

// Trigger garbage collection this many seconds after the last one.
const int kGarbageCollectionIntervalSeconds = 60;

SbOnceControl g_js_init_once_control = SB_ONCE_INITIALIZER;
v8::Platform* g_platform = nullptr;
v8::ArrayBuffer::Allocator* g_array_buffer_allocator = nullptr;

void ShutDown(void*) {
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();

  DCHECK(g_platform);
  delete g_platform;
  g_platform = nullptr;

  DCHECK(g_array_buffer_allocator);
  delete g_array_buffer_allocator;
  g_array_buffer_allocator = nullptr;
}

void InitializeAndRegisterShutDownOnce() {
  // TODO: Initialize V8 ICU stuff here as well.
  g_platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(g_platform);
  v8::V8::Initialize();
  g_array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();

  base::AtExitManager::RegisterCallback(ShutDown, nullptr);
}

}  // namespace

V8cEngine::V8cEngine(const Options& options)
    : accumulated_extra_memory_cost_(0), options_(options) {
  TRACE_EVENT0("cobalt::script", "V8cEngine::V8cEngine()");
  SbOnce(&g_js_init_once_control, InitializeAndRegisterShutDownOnce);
  DCHECK(g_platform);
  DCHECK(g_array_buffer_allocator);

  v8::Isolate::CreateParams params;
  params.array_buffer_allocator = g_array_buffer_allocator;

  isolate_ = v8::Isolate::New(params);
  CHECK(isolate_);

  if (MessageLoop::current()) {
    gc_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kGarbageCollectionIntervalSeconds), this,
        &V8cEngine::TimerGarbageCollect);
  }
}

V8cEngine::~V8cEngine() {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  NOTIMPLEMENTED();
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

void V8cEngine::TimerGarbageCollect() {
  TRACE_EVENT0("cobalt::script", "V8cEngine::TimerGarbageCollect()");
  CollectGarbage();
}

}  // namespace v8c

// static
scoped_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine(
    const JavaScriptEngine::Options& options) {
  TRACE_EVENT0("cobalt::script", "JavaScriptEngine::CreateEngine()");
  return make_scoped_ptr<JavaScriptEngine>(new v8c::V8cEngine(options));
}

// static
size_t JavaScriptEngine::UpdateMemoryStatsAndReturnReserved() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace script
}  // namespace cobalt

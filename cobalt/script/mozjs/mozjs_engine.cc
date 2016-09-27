/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/script/mozjs/mozjs_engine.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/poller.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/script/mozjs/mozjs_global_environment.h"
#include "third_party/mozjs/cobalt_config/include/jscustomallocator.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace {
// After this many bytes have been allocated, the garbage collector will run.
const uint32_t kGarbageCollectionThresholdBytes = 8 * 1024 * 1024;

JSBool CheckAccessStub(JSContext*, JS::Handle<JSObject*>, JS::Handle<jsid>,
                       JSAccessMode, JS::MutableHandle<JS::Value>) {
  return true;
}

JSSecurityCallbacks security_callbacks = {
  CheckAccessStub,
  MozjsGlobalEnvironment::CheckEval
};

#if defined(__LB_SHELL__FOR_RELEASE__)
const int kPollerPeriodMs = 2000;
#else  // #if defined(__LB_SHELL__FOR_RELEASE__)
const int kPollerPeriodMs = 20;
#endif  // #if defined(__LB_SHELL__FOR_RELEASE__)

class EngineStats {
 public:
  EngineStats();

  static EngineStats* GetInstance() {
    return Singleton<EngineStats,
                     StaticMemorySingletonTraits<EngineStats> >::get();
  }

  void EngineCreated() {
    base::AutoLock auto_lock(lock_);
    ++engine_count_;
  }

  void EngineDestroyed() {
    base::AutoLock auto_lock(lock_);
    --engine_count_;
  }

  void Update() {
    base::AutoLock auto_lock(lock_);
    allocated_memory_ =
        MemoryAllocatorReporter::Get()->GetCurrentBytesAllocated();
    mapped_memory_ = MemoryAllocatorReporter::Get()->GetCurrentBytesMapped();
    memory_sum_ = allocated_memory_ + mapped_memory_;
  }

 private:
  base::Lock lock_;
  base::CVal<size_t, base::CValPublic> allocated_memory_;
  base::CVal<size_t, base::CValPublic> mapped_memory_;
  base::CVal<size_t, base::CValPublic> memory_sum_;
  base::CVal<size_t> engine_count_;

  // Repeating timer to query the used bytes.
  scoped_ptr<base::PollerWithThread> poller_;
};

EngineStats::EngineStats()
    : allocated_memory_("Memory.JS.AllocatedMemory", 0,
                        "JS memory occupied by the Mozjs allocator."),
      mapped_memory_("Memory.JS.MappedMemory", 0, "JS mapped memory."),
      memory_sum_("Memory.JS", 0,
                  "Total memory occupied by the Mozjs allocator and heap."),
      engine_count_("Count.JS.Engine", 0,
                    "Total JavaScript engine registered.") {
  poller_.reset(new base::PollerWithThread(
      base::Bind(&EngineStats::Update, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kPollerPeriodMs)));
}

}  // namespace

MozjsEngine::MozjsEngine() {
  // TODO: Investigate the benefit of helper threads and things like
  // parallel compilation.
  runtime_ =
      JS_NewRuntime(kGarbageCollectionThresholdBytes, JS_NO_HELPER_THREADS);
  CHECK(runtime_);

  // Sets the size of the native stack that should not be exceeded.
  // Setting three quarters of the web module stack size to ensure that native
  // stack won't exceed the stack size.
  JS_SetNativeStackQuota(runtime_,
                         browser::WebModule::kWebModuleStackSize / 4 * 3);

  JS_SetRuntimePrivate(runtime_, this);

  JS_SetSecurityCallbacks(runtime_, &security_callbacks);

  // Use incremental garbage collection.
  JS_SetGCParameter(runtime_, JSGC_MODE, JSGC_MODE_INCREMENTAL);

  // Allow Spidermonkey to allocate as much memory as it needs. If this limit
  // is set we could limit the memory used by JS and "gracefully" restart,
  // for example.
  JS_SetGCParameter(runtime_, JSGC_MAX_BYTES, 0xffffffff);

  // Callback to be called whenever a JSContext is created or destroyed for this
  // JSRuntime.
  JS_SetContextCallback(runtime_, &MozjsEngine::ContextCallback);

  // Callback to be called at different points during garbage collection.
  JS_SetGCCallback(runtime_, &MozjsEngine::GCCallback);

  // Callback to be called during garbage collection during the sweep phase.
  JS_SetFinalizeCallback(runtime_, &MozjsEngine::FinalizeCallback);

  EngineStats::GetInstance()->EngineCreated();
}

MozjsEngine::~MozjsEngine() {
  DCHECK(thread_checker_.CalledOnValidThread());
  EngineStats::GetInstance()->EngineDestroyed();
  JS_DestroyRuntime(runtime_);
}

scoped_refptr<GlobalEnvironment> MozjsEngine::CreateGlobalEnvironment() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return new MozjsGlobalEnvironment(runtime_);
}

void MozjsEngine::CollectGarbage() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JS_GC(runtime_);
}

void MozjsEngine::ReportExtraMemoryCost(size_t bytes) { NOTIMPLEMENTED(); }

JSBool MozjsEngine::ContextCallback(JSContext* context, unsigned context_op) {
  JSRuntime* runtime = JS_GetRuntime(context);
  MozjsEngine* engine =
      static_cast<MozjsEngine*>(JS_GetRuntimePrivate(runtime));
  DCHECK(engine->thread_checker_.CalledOnValidThread());
  if (context_op == JSCONTEXT_NEW) {
    engine->contexts_.push_back(context);
  } else if (context_op == JSCONTEXT_DESTROY) {
    ContextVector::iterator it =
        std::find(engine->contexts_.begin(), engine->contexts_.end(), context);
    if (it != engine->contexts_.end()) {
      engine->contexts_.erase(it);
    }
  }
  return true;
}

void MozjsEngine::GCCallback(JSRuntime* runtime, JSGCStatus status) {
  MozjsEngine* engine =
      static_cast<MozjsEngine*>(JS_GetRuntimePrivate(runtime));
  for (int i = 0; i < engine->contexts_.size(); ++i) {
    MozjsGlobalEnvironment* global_environment =
        MozjsGlobalEnvironment::GetFromContext(engine->contexts_[i]);
    if (status == JSGC_BEGIN) {
      global_environment->BeginGarbageCollection();
    } else if (status == JSGC_END) {
      global_environment->EndGarbageCollection();
    }
  }
}

void MozjsEngine::FinalizeCallback(JSFreeOp* free_op, JSFinalizeStatus status,
                                   JSBool is_compartment) {
  MozjsEngine* engine =
      static_cast<MozjsEngine*>(JS_GetRuntimePrivate(free_op->runtime()));
  DCHECK(engine->thread_checker_.CalledOnValidThread());
  if (status == JSFINALIZE_GROUP_START) {
    for (int i = 0; i < engine->contexts_.size(); ++i) {
      MozjsGlobalEnvironment* global_environment =
          MozjsGlobalEnvironment::GetFromContext(engine->contexts_[i]);
      global_environment->DoSweep();
    }
  }
}

}  // namespace mozjs

scoped_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine() {
  return make_scoped_ptr<JavaScriptEngine>(new mozjs::MozjsEngine());
}

}  // namespace script
}  // namespace cobalt

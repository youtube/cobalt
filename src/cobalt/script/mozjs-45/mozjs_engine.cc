// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/script/mozjs-45/mozjs_engine.h"

#include <algorithm>
#include <string>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "cobalt/base/c_val.h"
#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "starboard/once.h"
#include "third_party/mozjs-45/js/public/Initialization.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {

namespace mozjs {
namespace {
// After this many bytes have been allocated, the garbage collector will run.
const uint32_t kGarbageCollectionThresholdBytes = 8 * 1024 * 1024;

// Trigger garbage collection this many seconds after the last one.
const int kGarbageCollectionIntervalSeconds = 60;

JSSecurityCallbacks security_callbacks = {
    MozjsGlobalEnvironment::CheckEval,  // contentSecurityPolicyAllows
    NULL,  // JSSubsumesOp - Added in SpiderMonkey 31
};

// DOM proxies have an extra slot for the expando object at index
// kJSProxySlotExpando.
// The expando object is a plain JSObject whose properties correspond to
// "expandos" (custom properties set by the script author).
// The exact value stored in the kJSProxySlotExpando slot depends on whether
// the interface is annotated with the [OverrideBuiltins] extended attribute.
const uint32_t kJSProxySlotExpando = 0;

// The DOMProxyShadowsCheck function will be called to check if the property for
// id should be gotten from the prototype, or if there is an own property that
// shadows it.
js::DOMProxyShadowsResult DOMProxyShadowsCheck(JSContext* context,
                                               JS::HandleObject proxy,
                                               JS::HandleId id) {
  DCHECK(IsProxy(proxy));
  JS::Value value = js::GetProxyExtra(proxy, kJSProxySlotExpando);
  DCHECK(value.isUndefined() || value.isObject());

  // [OverrideBuiltins] extended attribute is not supported.
  NOTIMPLEMENTED();

  // If DoesntShadow is returned then the slot at listBaseExpandoSlot should
  // either be undefined or point to an expando object that would contain the
  // own property.
  return js::DoesntShadow;
}

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

  size_t UpdateMemoryStatsAndReturnReserved() {
    base::AutoLock auto_lock(lock_);
    if (engine_count_.value() == 0) {
      return 0u;
    }

    return 0u;
  }

 private:
  base::Lock lock_;
  base::CVal<size_t, base::CValPublic> allocated_memory_;
  base::CVal<size_t, base::CValPublic> mapped_memory_;
  base::CVal<size_t> engine_count_;
};

EngineStats::EngineStats()
    : allocated_memory_("Memory.JS.AllocatedMemory", 0,
                        "JS memory occupied by the Mozjs allocator."),
      mapped_memory_("Memory.JS.MappedMemory", 0, "JS mapped memory."),
      engine_count_("Count.JS.Engine", 0,
                    "Total JavaScript engine registered.") {}

// Pretend we always preserve wrappers since we never call
// SetPreserveWrapperCallback anywhere else. This is necessary for
// TryPreserveReflector called by WeakMap to not crash. Disabling
// bindings to WeakMap does not appear to be an easy option because
// of its use in selfhosted.js. See bugzilla discussion linked where
// they decided to include a similar dummy in the mozjs shell.
// https://bugzilla.mozilla.org/show_bug.cgi?id=829798
bool DummyPreserveWrapperCallback(JSContext* cx, JSObject* obj) { return true; }

SbOnceControl g_js_init_once_control = SB_ONCE_INITIALIZER;

void CallShutDown(void*) { JS_ShutDown(); }

void CallInitAndRegisterShutDownOnce() {
  const bool js_init_result = JS_Init();
  CHECK(js_init_result);
  base::AtExitManager::RegisterCallback(CallShutDown, NULL);
}

void ReportErrorHandler(JSContext* context, const char* message,
                        JSErrorReport* report) {
  MozjsGlobalEnvironment* global_environment =
      MozjsGlobalEnvironment::GetFromContext(context);
  DCHECK(global_environment);
  global_environment->ReportError(message, report);
}

}  // namespace

MozjsEngine::MozjsEngine() : accumulated_extra_memory_cost_(0) {
  TRACE_EVENT0("cobalt::script", "MozjsEngine::MozjsEngine()");
  SbOnce(&g_js_init_once_control, CallInitAndRegisterShutDownOnce);
  runtime_ = JS_NewRuntime(kGarbageCollectionThresholdBytes);
  CHECK(runtime_);

  // Sets the size of the native stack that should not be exceeded.
  // Setting three quarters of the web module stack size to ensure that native
  // stack won't exceed the stack size.
  JS_SetNativeStackQuota(runtime_,
                         cobalt::browser::kWebModuleStackSize / 4 * 3);

  JS_SetSecurityCallbacks(runtime_, &security_callbacks);

  // Use incremental garbage collection.
  JS_SetGCParameter(runtime_, JSGC_MODE, JSGC_MODE_INCREMENTAL);

  // Allow Spidermonkey to allocate as much memory as it needs. If this limit
  // is set we could limit the memory used by JS and "gracefully" restart,
  // for example.
  JS_SetGCParameter(runtime_, JSGC_MAX_BYTES, 0xffffffff);

  // Callback to be called whenever a JSContext is created or destroyed for this
  // JSRuntime.
  JS_SetContextCallback(runtime_, &MozjsEngine::ContextCallback, this);

  // Callback to be called at different points during garbage collection.
  JS_SetGCCallback(runtime_, &MozjsEngine::GCCallback, this);

#if defined(ENGINE_SUPPORTS_JIT)
  const bool enable_jit = true;
  js::SetDOMProxyInformation(NULL /*domProxyHandlerFamily*/,
                             kJSProxySlotExpando, DOMProxyShadowsCheck);
#else
  const bool enable_jit = false;
#endif
  JS::RuntimeOptionsRef(runtime_)
      .setUnboxedArrays(true)
      .setBaseline(enable_jit)
      .setIon(enable_jit)
      .setAsmJS(enable_jit)
      .setNativeRegExp(enable_jit);

  // Callback to be called during garbage collection during the sweep phase.
  JS_AddFinalizeCallback(runtime_, &MozjsEngine::FinalizeCallback, this);

  js::SetPreserveWrapperCallback(runtime_, DummyPreserveWrapperCallback);

  JS_SetErrorReporter(runtime_, ReportErrorHandler);

  EngineStats::GetInstance()->EngineCreated();

  if (MessageLoop::current()) {
    gc_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(
                                   kGarbageCollectionIntervalSeconds),
                    this, &MozjsEngine::TimerGarbageCollect);
  }
}

MozjsEngine::~MozjsEngine() {
  DCHECK(thread_checker_.CalledOnValidThread());
  EngineStats::GetInstance()->EngineDestroyed();
  JS_DestroyRuntime(runtime_);
}

scoped_refptr<GlobalEnvironment> MozjsEngine::CreateGlobalEnvironment() {
  TRACE_EVENT0("cobalt::script", "MozjsEngine::CreateGlobalEnvironment()");
  DCHECK(thread_checker_.CalledOnValidThread());
  return new MozjsGlobalEnvironment(runtime_);
}

void MozjsEngine::CollectGarbage() {
  TRACE_EVENT0("cobalt::script", "MozjsEngine::CollectGarbage()");
  DCHECK(thread_checker_.CalledOnValidThread());
  JS_GC(runtime_);
}

void MozjsEngine::ReportExtraMemoryCost(size_t bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  accumulated_extra_memory_cost_ += bytes;
  if (accumulated_extra_memory_cost_ > kGarbageCollectionThresholdBytes) {
    accumulated_extra_memory_cost_ = 0;
    CollectGarbage();
  }
}

size_t MozjsEngine::UpdateMemoryStatsAndReturnReserved() {
  return EngineStats::GetInstance()->UpdateMemoryStatsAndReturnReserved();
}

bool MozjsEngine::RegisterErrorHandler(JavaScriptEngine::ErrorHandler handler) {
  error_handler_ = handler;
  JSDebugErrorHook hook = ErrorHookCallback;
  void* closure = this;
  JS_SetDebugErrorHook(runtime_, hook, closure);
  return true;
}

void MozjsEngine::TimerGarbageCollect() {
  TRACE_EVENT0("cobalt::script", "MozjsEngine::TimerGarbageCollect()");
  CollectGarbage();
}

bool MozjsEngine::ContextCallback(JSContext* context, unsigned context_op,
                                  void* data) {
  JSRuntime* runtime = JS_GetRuntime(context);
  MozjsEngine* engine = reinterpret_cast<MozjsEngine*>(data);
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

void MozjsEngine::GCCallback(JSRuntime* runtime, JSGCStatus status,
                             void* data) {
  MozjsEngine* engine = reinterpret_cast<MozjsEngine*>(data);
  if (status == JSGC_END) {
    engine->accumulated_extra_memory_cost_ = 0;
  }
  for (int i = 0; i < engine->contexts_.size(); ++i) {
    MozjsGlobalEnvironment* global_environment =
        MozjsGlobalEnvironment::GetFromContext(engine->contexts_[i]);
    if (status == JSGC_BEGIN) {
      TRACE_EVENT_BEGIN0("cobalt::script", "SpiderMonkey Garbage Collection");
      global_environment->BeginGarbageCollection();
    } else if (status == JSGC_END) {
      global_environment->EndGarbageCollection();
      TRACE_EVENT_END0("cobalt::script", "SpiderMonkey Garbage Collection");
    }
  }
}

void MozjsEngine::FinalizeCallback(JSFreeOp* free_op, JSFinalizeStatus status,
                                   bool is_compartment, void* data) {
  TRACE_EVENT0("cobalt::script", "MozjsEngine::FinalizeCallback()");
  MozjsEngine* engine = reinterpret_cast<MozjsEngine*>(data);
  DCHECK(engine->thread_checker_.CalledOnValidThread());
  if (status == JSFINALIZE_GROUP_START) {
    for (int i = 0; i < engine->contexts_.size(); ++i) {
      MozjsGlobalEnvironment* global_environment =
          MozjsGlobalEnvironment::GetFromContext(engine->contexts_[i]);
      global_environment->DoSweep();
    }
  }
}

JSBool MozjsEngine::ErrorHookCallback(JSContext* context, const char* message,
                                      JSErrorReport* report, void* closure) {
  MozjsEngine* this_ptr = static_cast<MozjsEngine*>(closure);
  return this_ptr->ReportJSError(context, message, report);
}

JSBool MozjsEngine::ReportJSError(JSContext* context, const char* message,
                                  JSErrorReport* report) {
  if (!error_handler_.is_null() && report && report->filename) {
    std::string file_name = report->filename;
    // Line/column can be zero for internal javascript exceptions. In this
    // case set the values to 1, otherwise the base::SourceLocation object
    // below will dcheck.
    int line = std::max<int>(1, report->lineno);
    int column = std::max<int>(1, report->column);
    if (file_name.empty()) {
      file_name = "<internal exception>";
    }
    base::SourceLocation source_location(file_name, line, column);
    error_handler_.Run(source_location, message);
  }
  return true;  // Allow error to propagate in the mozilla engine.
}

}  // namespace mozjs

scoped_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine() {
  TRACE_EVENT0("cobalt::script", "JavaScriptEngine::CreateEngine()");
  return make_scoped_ptr<JavaScriptEngine>(new mozjs::MozjsEngine());
}

}  // namespace script
}  // namespace cobalt
